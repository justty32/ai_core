#pragma once
// cli_internal.hpp — `llm` CLI 各 TU 共用的常數（單一真相源）。
// 對外介面在 cli.hpp；旗標機制在 cli_flags.*；config/檔案在 cli_config.*；orchestrator 在 cli.cpp。

namespace cli {

// ── 退出碼（語意見 cli.hpp 檔頭）──
inline constexpr int kExitOk = 0;       // 成功
inline constexpr int kExitUsage = 1;    // 用法錯：未知旗標／缺 prompt／檔案讀不到／config 壞
inline constexpr int kExitRequest = 2;  // 請求失敗：傳輸／後端
inline constexpr int kExitCancel = 130; // SIGINT 取消（128+SIGINT）

// ★ config 檔路徑的環境變數：只「指路」、不存設定值。前綴取執行檔名 llm、加 _CLI_ 收窄以避免撞
//   泛用的 LLM_。要改名只動這一行（config 邏輯與 --help 用法都引用此常數）。
inline constexpr const char *kConfigEnvVar = "LLM_CLI_CONFIG";

} // namespace cli
