#pragma once
// cli_config.hpp — 三層 config 來源解析（對齊 core-py 的 config.py／core/src/cli_config.cpp）。
//
// 來源優先序（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。本模組只處理「config 檔」這層
// （前二層）；命令列旗標覆寫在 cli.cpp。config 檔路徑：--config ＞ 環境變數 ＞ ~/.config/llm/config.json。

#include <string>

#include <cllm/llm.hpp> // llm::Client

namespace cli::config {

// 三層 config 前二層（對齊 config.py 的 load_config／cli_config.cpp 的 load_into）。回退出碼。
int load_into(llm::Client &client, bool has_config, const std::string &config_path);

} // namespace cli::config
