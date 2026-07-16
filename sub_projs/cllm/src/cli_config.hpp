#pragma once
// cli_config.hpp — `llm` CLI 的 config 檔載入 + 檔案/媒體小工具。
// config 三層來源（後者覆寫前者）：Client{} 內建預設 → config 檔（glaze 反射整份灌入）→ 命令列旗標。
// 前二層在此；命令列旗標覆寫在 cli.cpp 的 coerce 迴圈（走 cli_flags 的 assign_field）。

#include <string>

namespace llm::abi {
class Client;
} // namespace llm::abi

namespace cli::config {

// 讀整個檔到 out（二進位）。失敗回 false 並把原因寫進 err。
bool read_file(const std::string &path, std::string &out, std::string &err);

// 由副檔名猜 mime（夠 CLI 用；認不得就退成 octet-stream，交後端／data URI 自處理）。
std::string mime_of(const std::string &path);

// ~/.config/llm/config.json 的家目錄探測（沒 HOME 就回空）。
std::string default_config_path();

// 依「--config ＞ env kConfigEnvVar ＞ ~/.config/llm/config.json」解析路徑並載入 client。
//   前二者是「明指」→ 讀不到就報錯；家目錄那條是「探測」→ 不存在就靜默用預設（存在卻壞才報錯）。
// 回 kExitOk（0）＝成功（或無 config 檔）；回 kExitUsage＝載入/解析失敗（已印診斷到 stderr）。
int load_into(llm::abi::Client &client, bool has_config, const std::string &config_path);

} // namespace cli::config
