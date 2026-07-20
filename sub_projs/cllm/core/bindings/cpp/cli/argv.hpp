#pragma once
// argv.hpp — 命令列掃描：把 argv 拆成旗標與位置參數（對齊 core-py 的 argv.py／cli.cpp 解析段）。
//
// 固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/--media-out/--help/--）
// 特判；反射旗標（連線／取樣，見 cli_flags.hpp）吃下一個 argv；其餘當位置參數拼 prompt。「-」單獨
// ＝stdin 插入點，其餘 '-' 開頭＝未知旗標。結果收成 ParsedArgs；遇 --help／用法錯時把 stop 設真、
// 帶退出碼交呼叫端直接返回（main 只讀結果、不重演解析）。

#include <map>
#include <string>
#include <vector>

namespace cli::argv {

// argv 掃描結果：旗標收成的各欄位，交給 cli::run 續組 prompt／config／請求。
struct ParsedArgs {
  std::map<std::string, std::string> raw_values; // 反射欄位名 → 原始字串
  std::vector<std::string> prompt_parts, media_specs, tool_specs, modality_specs;
  std::string schema_text, config_path, media_out_dir, system_text;
  bool has_schema = false, has_config = false, has_system = false, stream = false;
  bool stop = false;  // 遇 --help／用法錯：呼叫端據此直接返回 exit_code
  int exit_code = 0;
};

// 掃描 argv（args[0]＝執行檔，從 1 起）。遇 --help／用法錯把 stop 設真並填 exit_code。
ParsedArgs parse_argv(const std::vector<std::string> &args);

} // namespace cli::argv
