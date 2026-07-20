#pragma once
// cli_internal.hpp — CLI 共用的退出碼、環境變數鍵、檔案讀取小工具
//   （對齊 core-py 的 internal.py／core/src/cli_internal.hpp）。
//
// 葉模組：只依標準庫、不 include 其他 cli 模組，作為其餘 CLI 模組（flags/config/media/
// output/argv/reqinput/cli）的共用底，把依賴收成一張無環 DAG。

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace cli {

// ── 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。──
inline constexpr int kExitOk = 0;       // 成功
inline constexpr int kExitUsage = 1;    // 用法錯：未知旗標／缺 prompt／檔案讀不到／config 壞
inline constexpr int kExitRequest = 2;  // 請求失敗：傳輸／後端／媒體落檔
inline constexpr int kExitCancel = 130; // SIGINT 取消（128+SIGINT）

// config 檔路徑的環境變數（只「指路」、不存設定值；對齊 kConfigEnvVar）。
inline constexpr const char *kConfigEnvVar = "LLM_CLI_CONFIG";

// 整檔讀成字串（C++ string 即位元組序列，bytes／text 同路）。讀不到擲 runtime_error，
// 由呼叫端捕捉轉退出碼（對齊 internal.py 的 read_file_bytes／read_file_text）。
inline std::string read_file(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    throw std::runtime_error("讀不到檔案：" + path);
  return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

} // namespace cli
