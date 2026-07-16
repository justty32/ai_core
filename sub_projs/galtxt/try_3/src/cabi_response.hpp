#pragma once
// cabi_response.hpp — C++ 薄鏡像的「收回」型＋出口回呼（對應 C ABI 的 cabi_response.h）。
// 純 C++；handler 用 std::function（閉包自帶狀態，故無 C ABI 的 void* user）。

#include <functional>
#include <string>
#include <string_view>

namespace llm::abi {

// 工具「呼叫」（模型回來要你執行的）。
struct ToolCall {
  std::string id, name, arguments;
};
// 產出媒體：模型產物（如生成的 audio），bytes＝原始位元組，種類看 mime。
struct MediaOut {
  std::string mime;
  std::string bytes;
};

// 出口回呼集（對應 llm_handlers_t）。用 std::function：閉包自帶狀態，故無 void* user。
// on_text/on_tool/on_media 回 true＝中止（對應 C ABI 回非 0）；任一可留空（＝該類輸出被丟棄）。
struct Handlers {
  std::function<bool(std::string_view)> on_text;  // 文字：串流逐段／非串流整段一次
  std::function<bool(const ToolCall &)> on_tool;  // 每個工具呼叫（拼完整）
  std::function<bool(const MediaOut &)> on_media; // 每則產出媒體
  std::function<void(std::string_view)> on_error; // 失敗時一次
};

} // namespace llm::abi
