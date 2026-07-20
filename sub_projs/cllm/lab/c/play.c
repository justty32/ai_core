/* play.c — cllm C ABI + jansson(JSON) + shell(CLI) 呼叫。
 * 編：cc play.c $(pkg-config --cflags --libs cllm jansson) -o example
 * 跑：source ~/dev/cllm/env.sh 後  ./play "$CLLM_FIXTURES" */
#include <stdio.h>
#include <string.h>
#include <cllm/cabi.h>
#include <jansson.h>

static char acc[8192]; static size_t acclen;
static int on_text(const char *t, size_t n, void *u) {
  (void)u; if (acclen + n < sizeof acc) { memcpy(acc + acclen, t, n); acclen += n; } return 0;
}

/* on_tool：模型回來的工具呼叫（arguments 是 JSON 字串，用 jansson 解回欄位再印）。*/
static int on_tool(const llm_tool_call_t *call, void *u) {
  (void)u;
  json_error_t err; json_t *args = json_loads(call->arguments, 0, &err);
  const char *city = args ? json_string_value(json_object_get(args, "city")) : NULL;
  const char *unit = args ? json_string_value(json_object_get(args, "unit")) : NULL;
  printf("[c] tool => %s(city=%s, unit=%s)\n", call->name, city ? city : "-", unit ? unit : "-");
  if (args) json_decref(args);
  return 0;
}

/* on_media：模型產出的媒體（如 audio），純位元組＋mime，長度看 len。*/
static int on_media(const llm_media_out_t *media, void *u) {
  (void)u;
  printf("[c] media out => mime=%s bytes=%zu\n", media->mime, media->len);
  return 0;
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
  /* 1) tools：組 llm_tool_def_t（手寫 parameters JSON schema）→ 打 fake_tool → on_tool 印結果 */
  {
    char url[1024]; snprintf(url, sizeof url, "%sfake_tool/chat/completions", base);
    llm_client_t c = {0}; c.endpoint = base[0] ? url : NULL;
    llm_tool_def_t tool = {
      .name = "get_weather",
      .description = "查某城市天氣",
      .parameters = "{\"type\":\"object\",\"properties\":{"
                     "\"city\":{\"type\":\"string\"},\"unit\":{\"type\":\"string\"}},"
                     "\"required\":[\"city\",\"unit\"]}"
    };
    llm_request_t r = {0}; r.prompt = "東京天氣如何？"; r.tools = &tool; r.tools_count = 1;
    llm_handlers_t h = {0}; h.on_tool = on_tool;
    llm_ask(NULL, &c, &r, &h);
  }

  /* 2) media 輸出：打 fake_media → on_media 印 mime/bytes */
  {
    char url[1024]; snprintf(url, sizeof url, "%sfake_media/chat/completions", base);
    llm_client_t c = {0}; c.endpoint = base[0] ? url : NULL;
    llm_request_t r = {0}; r.prompt = "說句話";
    llm_handlers_t h = {0}; h.on_media = on_media;
    llm_ask(NULL, &c, &r, &h);
  }

  /* 3) media 輸入＋modalities：掛上 request 打 fake（fixture 不看 body，只驗搬運不炸） */
  {
    char url[1024]; snprintf(url, sizeof url, "%sfake/chat/completions", base);
    llm_client_t c = {0}; c.endpoint = base[0] ? url : NULL;
    llm_media_in_t media_in = {.url = "data:image/png;base64,iVBORw0KGgo="};
    llm_modality_t modality = {.name = "audio", .config = "{\"voice\":\"alloy\",\"format\":\"wav\"}"};
    llm_request_t r = {0};
    r.prompt = "描述這張圖";
    r.media = &media_in; r.media_count = 1;
    r.modalities = &modality; r.modalities_count = 1;
    llm_handlers_t h = {0}; h.on_text = on_text; acclen = 0;
    llm_status_t st = llm_ask(NULL, &c, &r, &h);
    printf("[c] media in+modality => %s\n", st == LLM_OK ? "ok" : "failed");
  }

  /* shell 呼叫 llm CLI */
  char cmd[1024]; snprintf(cmd, sizeof cmd, "llm 你好 --endpoint '%sfake/chat/completions'", base);
  char buf[512] = {0}; FILE *p = popen(cmd, "r");
  if (p) { size_t n = fread(buf, 1, sizeof buf - 1, p); buf[n] = 0; pclose(p); }
  printf("[c] shell(llm) => %s", buf);
  return 0;
}
