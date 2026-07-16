/* example.c — cllm C ABI + jansson(JSON) + shell(CLI) 呼叫。
 * 編：cc example.c $(pkg-config --cflags --libs cllm jansson) -o example
 * 跑：source ~/repo/dev/env.sh 後  ./example "$CLLM_FIXTURES" */
#include <stdio.h>
#include <string.h>
#include <cllm/cabi.h>
#include <jansson.h>

static char acc[8192]; static size_t acclen;
static int on_text(const char *t, size_t n, void *u) {
  (void)u; if (acclen + n < sizeof acc) { memcpy(acc + acclen, t, n); acclen += n; } return 0;
}
static const char *ask(const char *base, const char *leaf, const char *prompt, const char *schema) {
  char url[1024]; snprintf(url, sizeof url, "%s%s", base, leaf);
  llm_client_t c = {0}; c.endpoint = base[0] ? url : NULL;
  llm_request_t r = {0}; r.prompt = prompt;
  llm_schema_t sc = {.name = "response", .json = schema};
  if (schema) r.schema = &sc;
  llm_handlers_t h = {0}; h.on_text = on_text; acclen = 0;
  llm_ask(NULL, &c, &r, &h); acc[acclen] = 0; return acc;
}
int main(int argc, char **argv) {
  const char *base = argc > 1 ? argv[1] : "";
  printf("[c] ask => %s\n", ask(base, "fake/chat/completions", "你好", NULL));

  /* schema → JSON → jansson 解析 */
  const char *raw = ask(base, "fake_json/chat/completions", "給我角色", "{\"type\":\"object\"}");
  json_error_t err; json_t *root = json_loads(raw, 0, &err);
  if (root) {
    printf("[c] json => name=%s affection=%lld lines=%zu\n",
           json_string_value(json_object_get(root, "name")),
           (long long)json_integer_value(json_object_get(root, "affection")),
           json_array_size(json_object_get(root, "lines")));
    json_decref(root);
  }
  /* shell 呼叫 llm CLI */
  char cmd[1024]; snprintf(cmd, sizeof cmd, "llm 你好 --endpoint '%sfake/chat/completions'", base);
  char buf[512] = {0}; FILE *p = popen(cmd, "r");
  if (p) { size_t n = fread(buf, 1, sizeof buf - 1, p); buf[n] = 0; pclose(p); }
  printf("[c] shell(llm) => %s", buf);
  return 0;
}
