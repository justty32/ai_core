#pragma once
// media.hpp — --image/--media 的 MIME 對照與三分流取值（對齊 core-py 的 media.py）。
//
// mime_of／ext_of 對齊 cli_config.cpp 的同名對照表；build_media_item 是取值分流：讓 --media 除了
// 讀二進位圖檔，也能吃「已編碼」形式（data:/http URL、或 .json 描述子），省掉每次重算 base64。

#include <optional>
#include <string>

#include <cllm/llm.hpp> // llm::abi::MediaIn

namespace cli::media {

// 副檔名 → MIME（對齊 cli_config.cpp 的 mime_of）。
std::string mime_of(const std::string &path);

// MIME → 落檔副檔名（對齊 cli_config.cpp 的 ext_of）。
std::string ext_of(const std::string &mime);

// 一個 --image/--media 值 → abi::MediaIn（三分流）。成功回值、失敗印 stderr 回 nullopt。
std::optional<llm::abi::MediaIn> build_media_item(const std::string &spec);

} // namespace cli::media
