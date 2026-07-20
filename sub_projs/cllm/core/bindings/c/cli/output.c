/* output.c — Sink 的四個出口回呼（對齊 cli_output.cpp）。回 0＝不主動中止。 */
#include "output.h"
#include "media.h"
#include <jansson.h>
#include <stdio.h>
#include <string.h>

/* 文字：串流逐段／非串流整段吐 stdout。 */
static int on_text(const char *text, size_t len, void *user) {
  sink_t *s = user;
  fwrite(text, 1, len, stdout);
  fflush(stdout);
  s->wrote_text = 1;
  return 0;
}

/* tool_calls 一行一則 JSON：{"tool","id","arguments"}（arguments 解回物件內嵌，壞則原樣塞字串）。 */
static int on_tool(const llm_tool_call_t *call, void *user) {
  (void)user;
  json_error_t err;
  json_t *args = call->arguments ? json_loads(call->arguments, 0, &err) : NULL;
  json_t *line = json_object();
  json_object_set_new(line, "tool", json_string(call->name ? call->name : ""));
  json_object_set_new(line, "id", json_string(call->id ? call->id : ""));
  json_object_set_new(line, "arguments", args ? args : json_string(call->arguments ? call->arguments : "{}"));
  char *dump = json_dumps(line, JSON_COMPACT); /* 預設輸出 UTF-8（不 ensure-ascii），中文原樣 */
  if (dump) { fputs(dump, stdout); fputc('\n', stdout); fflush(stdout); free(dump); }
  json_decref(line);
  return 0;
}

/* 產出媒體落檔 --media-out（llm-media-N.<ext>），路徑吐 stdout；沒給目錄＝明說丟棄。 */
static int on_media(const llm_media_out_t *m, void *user) {
  sink_t *s = user;
  if (!s->media_out_dir) {
    fprintf(stderr, "收到產出媒體（%s，%zu bytes）但沒給 --media-out，已丟棄\n", m->mime, m->len);
    return 0;
  }
  char path[1024];
  snprintf(path, sizeof path, "%s/llm-media-%d.%s", s->media_out_dir, ++s->media_n, ext_of(m->mime));
  FILE *f = fopen(path, "wb");
  if (!f || fwrite(m->data, 1, m->len, f) != m->len) {
    if (f) fclose(f);
    fprintf(stderr, "媒體落檔失敗：%s\n", path);
    s->media_err = 1;
    return 0;
  }
  fclose(f);
  fputs(path, stdout); fputc('\n', stdout); fflush(stdout);
  return 0;
}

/* 錯誤走 stderr，帶「請求失敗：」前綴。 */
static void on_error(const char *msg, size_t len, void *user) {
  sink_t *s = user;
  s->had_error = 1;
  fputs("請求失敗：", stderr);
  fwrite(msg, 1, len, stderr);
  fputc('\n', stderr);
}

void sink_bind(sink_t *s, llm_handlers_t *h) {
  memset(h, 0, sizeof *h);
  h->on_text = on_text; h->text_user = s;
  h->on_tool = on_tool; h->tool_user = s;
  h->on_media = on_media; h->media_user = s;
  h->on_error = on_error; h->error_user = s;
}
