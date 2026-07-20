/* cli.c — 薄 CLI 外殼：把命令列組成一次 llm_ask 的發問（鏡像 C++ 的 `llm` CLI）。
 *
 * 只做「參數解析 + I/O 接線」，真正的活（組請求／打 HTTP／解串流）全丟給內核 llm_ask。周邊拆到
 * 姊妹模組（皆對齊 C++ 分檔）：internal（cli_internal.hpp）＝退出碼／env／檔案讀取；flags
 * （cli_flags.cpp）＝反射旗標＋print_usage；argv＝argv 掃描（cli.cpp 解析段）；config
 * （cli_config.cpp）＝三層 config；client＝依欄位名寫 llm_client_t；media＝--media 三分流；
 * reqinput＝請求類旗標組成 llm_request_t 輸入（cli.cpp 組請求段）；output（cli_output.cpp）＝Sink。
 *
 * 退出碼：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。 */
#include "cli.h"
#include "argv.h"
#include "client.h"
#include "config.h"
#include "internal.h"
#include "output.h"
#include "reqinput.h"
#include <cllm/cabi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static llm_context_t g_ctx; /* 供 SIGINT handler 戳取消（阻塞的 llm_ask 在安全點收掉） */
void cli_request_cancel(void) { llm_cancel(&g_ctx); }

/* 讀整段 stdin（僅在被導管／檔案餵入時），去尾端換行。回 malloc 字串（可能空字串）。 */
static char *read_stdin_all(void) {
  size_t cap = 4096, n = 0;
  char *buf = malloc(cap);
  if (!buf) return NULL;
  size_t got;
  while ((got = fread(buf + n, 1, cap - n, stdin)) > 0) {
    n += got;
    if (n == cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
  }
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) n--; /* rstrip */
  buf[n] = 0;
  return buf;
}

/* 位置參數 × 導管 stdin 合體（對齊 cli.cpp）。回 malloc prompt（可能空）；stdin 為互動終端又用「-」回 NULL。 */
static char *build_prompt(const parsed_args_t *p, char *stdin_text, int stdin_is_tty) {
  int has_dash = 0;
  for (int i = 0; i < p->prompt_n; i++) if (!strcmp(p->prompt_parts[i], "-")) has_dash = 1;
  if (has_dash && stdin_is_tty) return NULL; /* 「-」須從導管／檔案讀 */
  char *body = NULL; size_t len = 0; FILE *m = open_memstream(&body, &len);
  for (int i = 0; i < p->prompt_n; i++) {
    if (i) fputc(' ', m);
    fputs(!strcmp(p->prompt_parts[i], "-") ? stdin_text : p->prompt_parts[i], m);
  }
  fflush(m);
  if (!has_dash && stdin_text[0]) { if (len) fputs("\n\n", m); fputs(stdin_text, m); } /* prompt＋空行＋stdin */
  fclose(m);
  return body;
}

int cli_main(int argc, char **argv) {
  parsed_args_t p;
  int ec = parse_argv(argc, argv, &p);
  if (ec != -1) return ec; /* --help 或用法錯 */

  int rc = EXIT_USAGE;
  char *stdin_text = NULL, *prompt = NULL;
  llm_client_t client = {0};
  owned_strings_t owned = {0};
  req_inputs_t r; memset(&r, 0, sizeof r);

  /* (2) prompt：位置參數與導管 stdin 可合體 */
  int tty = isatty(STDIN_FILENO);
  stdin_text = tty ? strdup("") : read_stdin_all();
  if (!stdin_text) { fputs("讀 stdin 失敗\n", stderr); goto done; }
  prompt = build_prompt(&p, stdin_text, tty);
  if (!prompt) { fputs("「-」要從 stdin 讀，但 stdin 是互動終端——用導管/檔案餵入（llm-c --help 看用法）\n", stderr); goto done; }
  if (!prompt[0]) { fputs("缺少 prompt：給位置參數或從 stdin 餵入（llm-c --help 看用法）\n", stderr); goto done; }

  /* (3) 組 client：內建預設 → config 檔 → 反射旗標覆寫 */
  if ((ec = load_config(&client, &owned, p.has_config, p.config_path)) != EXIT_OK) { rc = ec; goto done; }
  for (int i = 0; i < p.raw_n; i++) {
    raw_value_t *rv = &p.raw_values[i];
    if (client_set_from_str(&client, &owned, rv->field, (flag_type_t)rv->type, rv->value) != 0) {
      const char *kind = rv->type == F_FLOAT ? "小數" : rv->type == F_STR ? "字串" : "整數";
      fprintf(stderr, "%s 需要%s（給了「%s」）\n", rv->flag, kind, rv->value);
      goto done;
    }
  }

  /* (4) 組請求輸入 ＋ 呼叫內核 */
  if ((ec = build_request_inputs(&p, &r)) != EXIT_OK) { rc = ec; goto done; }
  llm_request_t req = {0};
  req.prompt = prompt;
  req.system = (p.has_system && p.system_text[0]) ? p.system_text : NULL;
  llm_schema_t sc = {"response", r.schema_json};
  if (r.schema_json) req.schema = &sc;
  req.tools = r.tools; req.tools_count = r.tools_count;
  req.media = r.media; req.media_count = r.media_count;
  req.modalities = r.modalities; req.modalities_count = r.modalities_count;
  req.stream = p.stream;

  sink_t sink = {0}; sink.media_out_dir = p.media_out_dir;
  llm_handlers_t h; sink_bind(&sink, &h);
  llm_status_t st = llm_ask(&g_ctx, &client, &req, &h);

  if (st == LLM_CANCELLED) { rc = EXIT_CANCEL; goto done; }
  int ok = !sink.had_error;
  if (sink.wrote_text && ok) { fputc('\n', stdout); fflush(stdout); } /* 補尾端換行 */
  rc = !ok ? EXIT_REQUEST : (sink.media_err ? EXIT_REQUEST : EXIT_OK);

done:
  reqinput_free(&r);
  owned_free(&owned);
  free(prompt); free(stdin_text);
  parsed_free(&p);
  return rc;
}
