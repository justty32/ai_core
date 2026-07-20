/* reqinput.c — 請求類旗標 → llm_request_t 輸入四件組（對齊 cli.cpp 組請求段＋core-py reqinput.py）。 */
#include "reqinput.h"
#include "media.h"
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void reqinput_free(req_inputs_t *r) {
  free(r->tools); free(r->media); free(r->modalities);
  cli_arena_free(&r->arena);
  memset(r, 0, sizeof *r);
}

/* 驗字面 JSON 合法（schema／modality config 用）。 */
static int json_ok(const char *text) {
  json_error_t e; json_t *j = json_loads(text, 0, &e);
  if (!j) return 0;
  json_decref(j);
  return 1;
}

/* 一個 --tool 值 → 填 *out。字面 JSON，需 name 與非空 parameters；parameters 是物件則序列化成字串。 */
static int build_tool(const char *spec, llm_tool_def_t *out, cli_arena_t *arena) {
  json_error_t e; json_t *td = json_loads(spec, 0, &e);
  if (!td || !json_is_object(td)) {
    if (td) json_decref(td);
    fputs("--tool 不是合法 JSON 物件（收字面文字非路徑；長內容用 $(cat t.json)）\n", stderr);
    return -1;
  }
  json_t *jn = json_object_get(td, "name"), *jp = json_object_get(td, "parameters");
  json_t *jd = json_object_get(td, "description");
  if (!json_is_string(jn) || !jp || json_is_null(jp)) {
    json_decref(td);
    fputs("--tool 缺 name 或 parameters\n", stderr);
    return -1;
  }
  memset(out, 0, sizeof *out);
  out->name = cli_arena_take(arena, strdup(json_string_value(jn)));
  out->description = cli_arena_take(arena, strdup(json_is_string(jd) ? json_string_value(jd) : ""));
  /* parameters：字串直接用，物件則 dump 成 JSON 字串（C ABI 要 const char*）。 */
  out->parameters = json_is_string(jp) ? cli_arena_take(arena, strdup(json_string_value(jp)))
                                       : cli_arena_take(arena, json_dumps(jp, JSON_COMPACT));
  json_decref(td);
  return 0;
}

/* 一個 --modality 值（「名」或「名=<字面JSON>」）→ 填 *out。 */
static int build_modality(const char *spec, llm_modality_t *out, cli_arena_t *arena) {
  const char *eq = strchr(spec, '=');
  size_t name_len = eq ? (size_t)(eq - spec) : strlen(spec);
  if (name_len == 0) { fputs("--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）\n", stderr); return -1; }
  char *name = malloc(name_len + 1);
  if (!name) return -1;
  memcpy(name, spec, name_len); name[name_len] = 0;
  memset(out, 0, sizeof *out);
  out->name = cli_arena_take(arena, name);
  if (eq) {
    if (!json_ok(eq + 1)) { fputs("--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）\n", stderr); return -1; }
    out->config = eq + 1; /* 指向 argv 尾段（NUL 結尾），生命週期夠 */
  }
  return 0;
}

int build_request_inputs(const parsed_args_t *p, req_inputs_t *r) {
  memset(r, 0, sizeof *r);
  if (p->has_schema) {
    if (!json_ok(p->schema_text)) { fputs("--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）\n", stderr); return EXIT_USAGE; }
    r->schema_json = p->schema_text;
  }
  if (p->tool_n) r->tools = calloc(p->tool_n, sizeof *r->tools);
  for (int i = 0; i < p->tool_n; i++) {
    if (build_tool(p->tool_specs[i], &r->tools[r->tools_count], &r->arena) != 0) return EXIT_USAGE;
    r->tools_count++;
  }
  if (p->modality_n) r->modalities = calloc(p->modality_n, sizeof *r->modalities);
  for (int i = 0; i < p->modality_n; i++) {
    if (build_modality(p->modality_specs[i], &r->modalities[r->modalities_count], &r->arena) != 0) return EXIT_USAGE;
    r->modalities_count++;
  }
  if (p->media_n) r->media = calloc(p->media_n, sizeof *r->media);
  for (int i = 0; i < p->media_n; i++) {
    if (build_media_item(p->media_specs[i], &r->media[r->media_count], &r->arena) != 0) return EXIT_USAGE;
    r->media_count++;
  }
  if (p->media_out_dir) {
    struct stat st;
    if (stat(p->media_out_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
      fprintf(stderr, "--media-out 不是可用目錄：%s\n", p->media_out_dir);
      return EXIT_USAGE;
    }
  }
  return EXIT_OK;
}
