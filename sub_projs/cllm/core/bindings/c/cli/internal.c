/* internal.c — 退出碼／env 鍵／檔案讀取小工具的實作（對齊 cli_internal.hpp）。 */
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>

/* 整檔讀進 malloc 緩衝。多配 1 byte 補 NUL，讓 bytes 也可當文字用。 */
static char *slurp(const char *path, size_t *len) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  long sz = ftell(f);
  if (sz < 0) { fclose(f); return NULL; }
  rewind(f);
  char *buf = malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t n = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[n] = 0;
  if (len) *len = n;
  return buf;
}

char *cli_read_file_bytes(const char *path, size_t *len) { return slurp(path, len); }

char *cli_read_file_text(const char *path) { return slurp(path, NULL); }

void *cli_arena_take(cli_arena_t *a, void *p) {
  if (!p) return NULL;
  if (a->n == a->cap) {
    size_t cap = a->cap ? a->cap * 2 : 8;
    void **items = realloc(a->items, cap * sizeof *items);
    if (!items) return p; /* 登記失敗也回 p，避免呼叫端誤判為錯；最壞是這塊漏掉 */
    a->items = items;
    a->cap = cap;
  }
  a->items[a->n++] = p;
  return p;
}

void cli_arena_free(cli_arena_t *a) {
  for (size_t i = 0; i < a->n; i++) free(a->items[i]);
  free(a->items);
  a->items = NULL;
  a->n = a->cap = 0;
}
