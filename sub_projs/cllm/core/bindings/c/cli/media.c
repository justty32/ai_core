/* media.c — MIME 對照 ＋ --image/--media 三分流取值（對齊 media.py）。 */
#include "media.h"
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */

static const char *ext_lower(const char *path) {
  const char *dot = strrchr(path, '.');
  return dot ? dot + 1 : "";
}

const char *mime_of(const char *path) {
  const char *e = ext_lower(path);
  if (!strcasecmp(e, "png")) return "image/png";
  if (!strcasecmp(e, "jpg") || !strcasecmp(e, "jpeg")) return "image/jpeg";
  if (!strcasecmp(e, "gif")) return "image/gif";
  if (!strcasecmp(e, "webp")) return "image/webp";
  if (!strcasecmp(e, "wav")) return "audio/wav";
  if (!strcasecmp(e, "mp3")) return "audio/mpeg";
  return "application/octet-stream";
}

const char *ext_of(const char *mime) {
  if (!strcmp(mime, "image/png")) return "png";
  if (!strcmp(mime, "image/jpeg")) return "jpg";
  if (!strcmp(mime, "image/gif")) return "gif";
  if (!strcmp(mime, "image/webp")) return "webp";
  if (!strcmp(mime, "audio/wav")) return "wav";
  if (!strcmp(mime, "audio/mpeg")) return "mp3";
  return "bin";
}

static int has_suffix_ci(const char *s, const char *suf) {
  size_t ls = strlen(s), lf = strlen(suf);
  return ls >= lf && !strcasecmp(s + ls - lf, suf);
}

/* .json 描述子：{"url":…} 直通，或 {"mime":…,"data":<base64>} 併成 data URI 當 url 送。 */
static int from_descriptor(const char *spec, llm_media_in_t *out, cli_arena_t *arena) {
  char *body = cli_read_file_text(spec);
  if (!body) { fprintf(stderr, "讀不到檔案：%s（--image/--media 描述子）\n", spec); return -1; }
  json_error_t err; json_t *d = json_loads(body, 0, &err); free(body);
  if (!d) { fprintf(stderr, "media 描述子 JSON 解析失敗（%s）\n", spec); return -1; }
  int rc = -1;
  json_t *ju = json_object_get(d, "url");
  json_t *jm = json_object_get(d, "mime"), *jd = json_object_get(d, "data");
  if (json_is_string(ju)) {
    out->url = cli_arena_take(arena, strdup(json_string_value(ju)));
    rc = 0;
  } else if (json_is_string(jm) && json_is_string(jd)) {
    const char *m = json_string_value(jm), *da = json_string_value(jd);
    size_t n = strlen(m) + strlen(da) + sizeof "data:;base64,";
    char *uri = malloc(n);
    if (uri) { snprintf(uri, n, "data:%s;base64,%s", m, da); out->url = cli_arena_take(arena, uri); rc = 0; }
  } else {
    fprintf(stderr, "media 描述子形狀不符——需 {\"url\":…} 或 {\"mime\":…,\"data\":…}（%s）\n", spec);
  }
  json_decref(d);
  return rc;
}

int build_media_item(const char *spec, llm_media_in_t *out, cli_arena_t *arena) {
  memset(out, 0, sizeof *out);
  if (!strncmp(spec, "data:", 5) || !strncasecmp(spec, "http://", 7) || !strncasecmp(spec, "https://", 8)) {
    out->url = spec; /* URL 直通、不編碼（argv 生命週期即可） */
    return 0;
  }
  if (has_suffix_ci(spec, ".json")) return from_descriptor(spec, out, arena);
  /* 二進位圖檔：讀檔 + 內核 base64（data/len＋mime，url 留 NULL）。 */
  size_t len = 0;
  char *raw = cli_read_file_bytes(spec, &len);
  if (!raw) { fprintf(stderr, "讀不到檔案：%s（--image/--media）\n", spec); return -1; }
  out->data = cli_arena_take(arena, raw);
  out->len = len;
  out->mime = mime_of(spec);
  return 0;
}
