#pragma once
// cli.hpp — galtxt try_3 的 `llm` CLI（unix filter）介面。
//
// 這是對外 C ABI（libgaltxt.so）之上的第二個交付物：一支 unix 風格的過濾器執行檔。
//   · prompt 走位置參數；沒給就從 stdin 整段讀（`llm 你好` 或 `echo 你好 | llm`）。
//   · 答案吐 stdout；診斷／錯誤吐 stderr；退出碼分三段（見下）。
//   · 消費路徑＝C++ 薄鏡像 cabi.hpp（llm::abi::Client），連結 libgaltxt。
//
// ★ 旗標分兩類：
//   (1) 固定旗標（手寫）：--stream／--image <檔>（可重複）／--schema <檔>／--config <檔>／--help。
//   (2) 取樣／連線旗標：從 llm::abi::Client 的欄位「反射生成」（--endpoint／--model／--temperature…）。
//       要多一個旋鈕＝在 Client 加一個欄位，旗標與 --help 自動長出來、零改動（沿用舊 cli 的作法）。
//
// ★ 設定來源（無 env 存值，只有 env 指路）：Client{} 預設 → config 檔（glaze 反射）→ 命令列旗標覆寫。
//   config 檔解析順序見 cli.cpp；env LLM_CLI_CONFIG 只用來「指定 config 檔路徑」，不存任何設定值。
//
// 退出碼：0＝成功；1＝用法錯（未知旗標／缺 prompt／檔案讀不到／config JSON 壞）；
//         2＝請求失敗（傳輸／後端）；130＝收到 SIGINT 取消（128+SIGINT，POSIX）。

#include <string>
#include <vector>

namespace cli {

// 跑 CLI。args＝完整命令列（args[0]＝執行檔，內部略過）。回行程結束碼（見檔頭退出碼表）。
int run(const std::vector<std::string> &args);

} // namespace cli
