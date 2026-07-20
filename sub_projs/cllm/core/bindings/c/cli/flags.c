/* flags.c — 反射旗標表、flag→欄位查找、--help（對齊 cli_flags.cpp）。 */
#include "flags.h"
#include "internal.h"
#include <string.h>

/* 順序／欄位對齊 llm_client_t（cabi_client.h）。 */
const reflect_flag_t REFLECT_FLAGS[] = {
  {"--endpoint", "endpoint", F_STR},
  {"--api-key", "api_key", F_STR},
  {"--timeout-ms", "timeout_ms", F_LONG},
  {"--model", "model", F_STR},
  {"--temperature", "temperature", F_FLOAT},
  {"--top-p", "top_p", F_FLOAT},
  {"--presence-penalty", "presence_penalty", F_FLOAT},
  {"--frequency-penalty", "frequency_penalty", F_FLOAT},
  {"--max-tokens", "max_tokens", F_INT},
  {"--seed", "seed", F_INT},
};
const int REFLECT_FLAGS_COUNT = (int)(sizeof REFLECT_FLAGS / sizeof REFLECT_FLAGS[0]);

const reflect_flag_t *flag_lookup(const char *flag) {
  for (int i = 0; i < REFLECT_FLAGS_COUNT; i++)
    if (!strcmp(REFLECT_FLAGS[i].flag, flag)) return &REFLECT_FLAGS[i];
  return NULL;
}

/* --help 用的欄位標註（對齊 cli_flags.cpp 的 field_annot；範圍為 OpenAI 慣例值）。 */
static const char *annot_of(const char *field) {
  if (!strcmp(field, "temperature")) return "，範圍 0.0–2.0，例 0.7（越大越發散）";
  if (!strcmp(field, "top_p")) return "，範圍 0.0–1.0，例 0.9（與 temperature 二擇一）";
  if (!strcmp(field, "presence_penalty")) return "，範圍 -2.0–2.0，例 0.0";
  if (!strcmp(field, "frequency_penalty")) return "，範圍 -2.0–2.0，例 0.0";
  if (!strcmp(field, "max_tokens")) return "，≥1，例 512（⚠ reasoning 模型建議不設）";
  if (!strcmp(field, "seed")) return "，例 42（固定可重現）";
  if (!strcmp(field, "timeout_ms")) return "，≥0（0＝不設限），例 120000";
  if (!strcmp(field, "endpoint")) return "，例 http://localhost:1234/v1/chat/completions";
  if (!strcmp(field, "model")) return "，例 local-model（雲端填真實 model id）";
  if (!strcmp(field, "api_key")) return "，雲端 API 必給";
  return "";
}

static const char *hint_of(flag_type_t t) {
  return t == F_STR ? "<字串>" : t == F_FLOAT ? "<小數>" : "<整數>";
}

void print_usage(FILE *out) {
  fputs(
    "用法：llm-c [旗標...] [prompt...]      # 旗標與 prompt 可交錯\n"
    "  prompt 來源：位置參數＋導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有\n"
    "  ＝prompt＋空行＋stdin（指令在前、資料在後）；只給其一＝用那一個。\n"
    "  範例：llm-c 用一句話介紹你自己\n"
    "        cat report.txt | llm-c 總結這份       # prompt＋stdin 合體\n"
    "        git diff | llm-c 把 - 寫成 commit 訊息 # 「-」明指 stdin 插入點\n"
    "        llm-c --stream -- --開頭的-prompt      # -- 之後一律當 prompt（unix 分隔符）\n"
    "\n固定旗標：\n"
    "  --system <文字>        system role 指示（字面文字；長文用 --system \"$(cat persona.txt)\"）\n"
    "  --stream               串流逐段吐 stdout（布林，無值）\n"
    "  --image/--media <值>   夾帶輸入媒體（可重複），三分流：data:/http(s):// URL 直接當 url 送；\n"
    "                         .json 檔＝預先算好的 media 描述子（{\"url\":…} 或 {\"mime\":…,\"data\":…}）\n"
    "                         直通；其餘＝二進位圖檔路徑，讀檔＋base64（mime 由副檔名推）\n"
    "  --schema <JSON>        字面 JSON Schema 文字，要求結構化輸出（⚠ 收字面非路徑；長內容用 $(cat s.json)）\n"
    "  --config <檔>          設定檔（扁平 JSON，對應下列連線／取樣欄位）\n"
    "  --tool <JSON>          字面工具定義 JSON（可重複）：{\"name\",\"description\",\"parameters\"}\n"
    "  --modality <名[=JSON]> 要求輸出模態（可重複）：如 audio 或 audio={\"voice\":\"alloy\"}\n"
    "  --media-out <目錄>     產出媒體落檔目錄（llm-media-N.<ext>，路徑逐行吐 stdout）；沒給＝丟棄\n"
    "  --                     分隔符：其後所有參數一律當 prompt\n"
    "  --help, -h             顯示本說明\n"
    "\n連線／取樣旗標（對應 llm_client_t 欄位，未給即不送、交後端默認）：\n", out);
  for (int i = 0; i < REFLECT_FLAGS_COUNT; i++)
    fprintf(out, "  %-22s %s%s\n", REFLECT_FLAGS[i].flag,
            hint_of(REFLECT_FLAGS[i].type), annot_of(REFLECT_FLAGS[i].field));
  fprintf(out,
    "\n設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。\n"
    "config 檔路徑：--config <檔> ＞ 環境變數 %s ＞ ~/.config/llm/config.json。\n"
    "離線自測：--endpoint file://<絕對路徑> 指向 test/fixtures 的假回應。\n", CONFIG_ENV_VAR);
}
