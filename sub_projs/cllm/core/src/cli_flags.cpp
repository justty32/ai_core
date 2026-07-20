// cli_flags.cpp — cli_flags.hpp 的非模板實作：反射 Client 生旗標＋印 --help。
//
// 為什麼配 idx++ 就能拿到欄位名：glz::for_each_field 用 fold-over-逗號運算子依序呼叫 callable
//   （由左到右求值），故第 k 次收到的欄位對應 reflect::keys[k]。

#include "cli_flags.hpp"

#include <cstdio>

#include <glaze/glaze.hpp> // reflect<T>::keys / for_each_field
#include "cabi.hpp"        // llm::abi::Client
#include "cli_internal.hpp" // kConfigEnvVar（--help 用法引用）

namespace cli::flags {

std::string kebab_flag(std::string_view name) {
  std::string s = "--";
  for (char c : name)
    s += (c == '_') ? '-' : c;
  return s;
}

std::vector<FlagInfo> client_flags() {
  std::vector<FlagInfo> out;
  llm::abi::Client probe{};
  std::size_t idx = 0;
  glz::for_each_field(probe, [&](auto &&f) {
    using T = std::remove_cvref_t<decltype(f)>;
    std::string_view name = glz::reflect<llm::abi::Client>::keys[idx++];
    out.push_back(FlagInfo{kebab_flag(name), std::string(name), type_hint<T>()});
  });
  return out;
}

namespace {
// 反射給不出「數值上下限／範例」這種語意——用一張以欄位名為鍵的標註表補上（--help 專用）。
// 範圍是 OpenAI 慣例值（實際上下限依後端而定）；沒列到的欄位退回只印型別提示。
const char *field_annot(std::string_view name) {
  if (name == "temperature")       return "，範圍 0.0–2.0，例 0.7（越大越發散）";
  if (name == "top_p")             return "，範圍 0.0–1.0，例 0.9（與 temperature 二擇一）";
  if (name == "presence_penalty")  return "，範圍 -2.0–2.0，例 0.0";
  if (name == "frequency_penalty") return "，範圍 -2.0–2.0，例 0.0";
  if (name == "max_tokens")        return "，≥1，例 512（⚠ reasoning 模型建議不設）";
  if (name == "seed")              return "，例 42（固定可重現）";
  if (name == "timeout_ms")        return "，≥0（0＝不設限），例 120000";
  if (name == "endpoint")          return "，例 http://localhost:1234/v1/chat/completions";
  if (name == "model")             return "，例 local-model（雲端填真實 model id）";
  if (name == "api_key")           return "，雲端 API 必給";
  return "";
}
} // namespace

void print_usage() {
  std::fprintf(stderr,
      "用法：llm [旗標...] [prompt...]        # 旗標與 prompt 可交錯\n"
      "  prompt 來源：位置參數＋導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有\n"
      "  ＝prompt＋空行＋stdin（指令在前、資料在後）；只給其一＝用那一個。\n"
      "  範例：llm 用一句話介紹你自己\n"
      "        cat report.txt | llm 總結這份       # prompt＋stdin 合體\n"
      "        git diff | llm 把 - 寫成 commit 訊息 # 「-」明指 stdin 插入點\n"
      "        llm --stream -- --開頭的-prompt      # -- 之後一律當 prompt（unix 分隔符）\n"
      "\n固定旗標：\n"
      "  --system <文字>        system role 指示（字面文字；長文用 --system \"$(cat persona.txt)\"）\n"
      "  --stream               串流逐段吐 stdout（布林，無值）\n"
      "  --image <檔>           夾帶輸入媒體（可重複；mime 由副檔名推）\n"
      "  --schema <檔>          JSON Schema 檔，要求結構化輸出\n"
      "  --config <檔>          設定檔（扁平 JSON，對應下列連線／取樣欄位）\n"
      "  --tool <檔>            工具定義檔（可重複）：{\"name\",\"description\",\"parameters\"}；\n"
      "                         模型的 tool_calls 以一行一則 JSON 吐 stdout（jq 友善）\n"
      "  --modality <名[=檔]>   要求輸出模態（可重複）：如 audio 或 audio=cfg.json（模態參數 JSON）\n"
      "  --media-out <目錄>     產出媒體落檔目錄（llm-media-N.<ext>，路徑逐行吐 stdout）；沒給＝丟棄\n"
      "  --                     分隔符：其後所有參數一律當 prompt\n"
      "  --help, -h             顯示本說明\n"
      "\n連線／取樣旗標（由 llm::abi::Client 欄位反射生成，未給即不送、交後端默認）：\n");
  for (const auto &fi : client_flags())
    std::fprintf(stderr, "  %-22s %s%s\n", fi.flag.c_str(), fi.hint.c_str(),
                 field_annot(fi.field));
  std::fprintf(stderr,
      "\n（數值範圍為 OpenAI 慣例，實際上下限依後端而定。）\n"
      "\n設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。\n"
      "config 檔路徑：--config <檔> ＞ 環境變數 %s ＞ ~/.config/llm/config.json。\n"
      "  （env 只用來指定 config 檔路徑，不存任何設定值。）\n"
      "離線自測：--endpoint file://<絕對路徑> 指向 test/fixtures 的假回應。\n",
      kConfigEnvVar);
}

} // namespace cli::flags
