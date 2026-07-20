/* config.c — 三層 config 來源前二層（對齊 cli_config.cpp 的 load_into）。用 jansson 解檔。
 * 未列鍵靜默忽略（lenient；與 C++ glaze 嚴格解析刻意分歧，見 core-py README 已知落差）。 */
#include "config.h"
#include "flags.h"
#include "internal.h"
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ~/.config/llm/config.json（對齊 cli_config.cpp：靠 HOME）。回 malloc 字串或 NULL。 */
static char *default_config_path(void) {
  const char *home = getenv("HOME");
  if (!home) return NULL;
  size_t n = strlen(home) + sizeof "/.config/llm/config.json";
  char *p = malloc(n);
  if (p) snprintf(p, n, "%s/.config/llm/config.json", home);
  return p;
}

/* 把一個 config 欄位（jansson 值）依型別灌進 client。 */
static void apply_key(llm_client_t *c, owned_strings_t *o, const reflect_flag_t *rf, json_t *v) {
  if (rf->type == F_STR) {
    if (json_is_string(v)) client_set_from_str(c, o, rf->field, F_STR, json_string_value(v));
  } else if (json_is_number(v)) {
    client_set_number(c, rf->field, json_number_value(v));
  }
}

int load_config(llm_client_t *c, owned_strings_t *o, int has_config, const char *config_path) {
  int named = 0;
  const char *cfg_path;
  char *owned_path = NULL;
  if (has_config) { cfg_path = config_path; named = 1; }
  else if (getenv(CONFIG_ENV_VAR) && getenv(CONFIG_ENV_VAR)[0]) { cfg_path = getenv(CONFIG_ENV_VAR); named = 1; }
  else { owned_path = default_config_path(); cfg_path = owned_path; }
  if (!cfg_path) return EXIT_OK;

  char *body = cli_read_file_text(cfg_path);
  if (!body) {
    if (named) { /* 明指卻讀不到＝用法錯（點名是誰指的路） */
      fprintf(stderr, "讀不到檔案：%s（%s 指定的 config 檔）\n",
              cfg_path, has_config ? "--config" : CONFIG_ENV_VAR);
      free(owned_path);
      return EXIT_USAGE;
    }
    free(owned_path);
    return EXIT_OK; /* 探測路徑讀不到＝沒設定檔，靜默用預設 */
  }
  free(owned_path);

  json_error_t err;
  json_t *root = json_loads(body, 0, &err);
  free(body);
  if (!root) { fprintf(stderr, "config JSON 解析失敗（%s）\n", cfg_path); return EXIT_USAGE; }
  if (json_is_object(root)) {
    const char *key; json_t *v;
    json_object_foreach(root, key, v) {
      const reflect_flag_t *rf = NULL;
      for (int i = 0; i < REFLECT_FLAGS_COUNT; i++)
        if (!strcmp(REFLECT_FLAGS[i].field, key)) { rf = &REFLECT_FLAGS[i]; break; }
      if (rf) apply_key(c, o, rf, v); /* 未列鍵靜默忽略 */
    }
  }
  json_decref(root);
  return EXIT_OK;
}
