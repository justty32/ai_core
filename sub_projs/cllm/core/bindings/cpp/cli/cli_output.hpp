#pragma once
// cli_output.hpp — 四個出口打包成 Sink（對齊 core-py 的 output.py／core/src/cli_output.cpp）。
//
// 把「輸出接線＋其共享狀態」收成一物件：文字吐 stdout、tool_calls 一行一則 JSON、產出媒體落檔
// --media-out、錯誤吐 stderr。on_delta 走 llm::AskOpts 逐段回呼；tool_calls／media 由 cli.cpp
// 從聚合後的 Reply 依序交給 emit_tool_call／save_media。收尾狀態（wrote_text／media_err）定退出碼。

#include <string>
#include <string_view>

#include <cllm/llm.hpp> // llm::abi::ToolCall / MediaOut

namespace cli::output {

struct Sink {
  std::string media_out_dir;
  bool wrote_text = false; // 有無吐過文字（決定要不要補尾端換行）
  bool media_err = false;  // 媒體落檔失敗＝結果真掉了
  int media_n = 0;         // 已落檔媒體數（供編號檔名）

  // 逐段文字 → stdout（llm::AskOpts.on_delta；回 false 不主動中止）。
  bool on_delta(std::string_view sv);
  // tool_calls 一行一則 JSON（{"tool","id","arguments"}；arguments 原樣內嵌）。
  void emit_tool_call(const llm::abi::ToolCall &tc);
  // 產出媒體落檔 --media-out（路徑吐 stdout）；沒給目錄＝明說丟棄。
  void save_media(const llm::abi::MediaOut &m);
};

} // namespace cli::output
