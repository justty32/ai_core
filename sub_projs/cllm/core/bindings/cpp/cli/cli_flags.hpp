#pragma once
// cli_flags.hpp — 反射旗標表與 --help 用法文字（對齊 core-py 的 flags.py／core/src/cli_flags.cpp）。
//
// 反射旗標＝連線／取樣選項，逐項對應 llm::abi::Client 欄位（順序／欄位對齊 cabi.hpp）；固定旗標
// （--stream/--image/…）的解析在 argv.hpp。print_usage 印的清單即由本表生成。
// header-only（葉＋純表）：table 與 print_usage 皆 inline，跨 TU 由 ODR-inline 保證單一定義。

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include <cllm/llm.hpp>     // llm::Client（coerce 目標）

#include "cli_internal.hpp" // kConfigEnvVar（--help 用法引用）

namespace cli::flags {

// 旗標值型別（決定 argv 值怎麼 coerce 進 Client 欄位）。
enum class Type { Str, Int, Float };

struct FlagInfo {
  const char *flag;  // --endpoint
  const char *field; // endpoint（對應 Client 欄位）
  Type type;
  const char *hint;  // 型別提示 <字串> 等
  const char *annot; // --help 標註（範圍／範例，OpenAI 慣例值）
};

// 反射旗標表（對齊 REFLECT_FLAGS／client_flags；順序、欄位對齊 llm::abi::Client）。
inline const std::vector<FlagInfo> &client_flags() {
  static const std::vector<FlagInfo> table = {
      {"--endpoint", "endpoint", Type::Str, "<字串>", "，例 http://localhost:1234/v1/chat/completions"},
      {"--api-key", "api_key", Type::Str, "<字串>", "，雲端 API 必給"},
      {"--timeout-ms", "timeout_ms", Type::Int, "<整數>", "，≥0（0＝不設限），例 120000"},
      {"--model", "model", Type::Str, "<字串>", "，例 local-model（雲端填真實 model id）"},
      {"--temperature", "temperature", Type::Float, "<小數>", "，範圍 0.0–2.0，例 0.7（越大越發散）"},
      {"--top-p", "top_p", Type::Float, "<小數>", "，範圍 0.0–1.0，例 0.9（與 temperature 二擇一）"},
      {"--presence-penalty", "presence_penalty", Type::Float, "<小數>", "，範圍 -2.0–2.0，例 0.0"},
      {"--frequency-penalty", "frequency_penalty", Type::Float, "<小數>", "，範圍 -2.0–2.0，例 0.0"},
      {"--max-tokens", "max_tokens", Type::Int, "<整數>", "，≥1，例 512（⚠ reasoning 模型建議不設）"},
      {"--seed", "seed", Type::Int, "<整數>", "，例 42（固定可重現）"},
  };
  return table;
}

// 印 --help（對齊 flags.py 的 print_usage／cli_flags.cpp；輸出到 stderr）。
inline void print_usage() {
  std::fprintf(stderr,
      "用法：llm-cpp [旗標...] [prompt...]     # 旗標與 prompt 可交錯\n"
      "  prompt 來源：位置參數＋導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有\n"
      "  ＝prompt＋空行＋stdin（指令在前、資料在後）；只給其一＝用那一個。\n"
      "  範例：llm-cpp 用一句話介紹你自己\n"
      "        cat report.txt | llm-cpp 總結這份       # prompt＋stdin 合體\n"
      "        git diff | llm-cpp 把 - 寫成 commit 訊息 # 「-」明指 stdin 插入點\n"
      "        llm-cpp --stream -- --開頭的-prompt      # -- 之後一律當 prompt（unix 分隔符）\n"
      "\n固定旗標：\n"
      "  --system <文字>        system role 指示（字面文字；長文用 --system \"$(cat persona.txt)\"）\n"
      "  --stream               串流逐段吐 stdout（布林，無值）\n"
      "  --image/--media <值>   夾帶輸入媒體（可重複），三分流：data:/http(s):// URL 直接當 url 送；\n"
      "                         .json 檔＝預先算好的 media 描述子（{\"url\":…} 或 {\"mime\":…,\"data\":…}）\n"
      "                         直通、不重算 base64；其餘＝二進位圖檔路徑，讀檔＋bytes（mime 由副檔名推）\n"
      "  --schema <JSON>        字面 JSON Schema 文字，要求結構化輸出（⚠ 收字面非路徑；長內容用 $(cat s.json)）\n"
      "  --config <檔>          設定檔（扁平 JSON，對應下列連線／取樣欄位）\n"
      "  --tool <JSON>          字面工具定義 JSON（可重複）：{\"name\",\"description\",\"parameters\"}；\n"
      "                         ⚠ 收字面非路徑，長內容用 $(cat t.json)；tool_calls 一行一則 JSON 吐 stdout\n"
      "  --modality <名[=JSON]> 要求輸出模態（可重複）：如 audio 或 audio={\"voice\":\"alloy\"}（=後收字面 JSON）\n"
      "  --media-out <目錄>     產出媒體落檔目錄（llm-media-N.<ext>，路徑逐行吐 stdout）；沒給＝丟棄\n"
      "  --                     分隔符：其後所有參數一律當 prompt\n"
      "  --help, -h             顯示本說明\n"
      "\n連線／取樣旗標（對應 llm::abi::Client 欄位，未給即不送、交後端默認）：\n");
  static const char *hintstr[] = {"<字串>", "<整數>", "<小數>"};
  for (const auto &fi : client_flags())
    std::fprintf(stderr, "  %-22s %s%s\n", fi.flag, hintstr[static_cast<int>(fi.type)], fi.annot);
  std::fprintf(stderr,
      "\n（數值範圍為 OpenAI 慣例，實際上下限依後端而定。）\n"
      "\n設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。\n"
      "config 檔路徑：--config <檔> ＞ 環境變數 %s ＞ ~/.config/llm/config.json。\n"
      "  （env 只用來指定 config 檔路徑，不存任何設定值。）\n"
      "離線自測：--endpoint file://<絕對路徑> 指向 test/fixtures 的假回應。\n",
      kConfigEnvVar);
}

// 反射旗標 coerce：raw_values（欄位名→字串）依型別灌進 Client（覆寫 config）。失敗印 stderr 回 false
//   （對齊 cli.cpp 的 for_each_field coerce 段／cli.py 的 raw_values 迴圈）。
inline bool apply_client_flags(llm::Client &client, const std::map<std::string, std::string> &raw) {
  auto is = [](const char *field, const char *lit) { return std::string(field) == lit; };
  for (const auto &fi : client_flags()) {
    auto it = raw.find(fi.field);
    if (it == raw.end())
      continue;
    const std::string &v = it->second;
    try {
      switch (fi.type) {
      case Type::Str:
        if (is(fi.field, "endpoint")) client.endpoint = v;
        else if (is(fi.field, "api_key")) client.api_key = v;
        else client.model = v; // model
        break;
      case Type::Int:
        if (is(fi.field, "timeout_ms")) client.timeout_ms = std::stol(v);
        else if (is(fi.field, "max_tokens")) client.max_tokens = std::stoi(v);
        else client.seed = std::stoi(v);
        break;
      case Type::Float:
        if (is(fi.field, "temperature")) client.temperature = std::stof(v);
        else if (is(fi.field, "top_p")) client.top_p = std::stof(v);
        else if (is(fi.field, "presence_penalty")) client.presence_penalty = std::stof(v);
        else client.frequency_penalty = std::stof(v);
        break;
      }
    } catch (const std::exception &) {
      std::fprintf(stderr, "%s 需要%s（給了「%s」）\n", fi.flag,
                   fi.type == Type::Int ? "整數" : "小數", v.c_str());
      return false;
    }
  }
  return true;
}

} // namespace cli::flags
