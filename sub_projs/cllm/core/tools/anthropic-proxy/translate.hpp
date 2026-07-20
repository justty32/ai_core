// translate.hpp — OpenAI chat/completions ⇄ Anthropic Messages 純翻譯層（介面）。
//
// cllm 說 OpenAI＋Bearer；Anthropic 收 Messages＋x-api-key。這裡把兩邊形狀接起來。
// 用 glaze 的 json_t（泛型 JSON DOM）做動態重塑——這是兩種**外部** wire format 之間的
// 翻譯 shim，本質動態，用 json_t 比硬打型別 struct 精簡；cllm 自家 ABI 才用反射 struct。
// 無 I/O、無網路、可離線測（見 test_offline.cpp）。契約來源見 ../../src/cabi_*.cpp。

#pragma once
#include <set>
#include <string>
#include <vector>

#include <glaze/glaze.hpp>

namespace axl {  // anthropic translate layer

using J = glz::json_t;

// response_format(json_schema) 無 Messages 原生對應 → 塞這個名字的「強制工具」＋tool_choice
// 逼模型呼叫，回來時把 input 當 content JSON 字串交還（還原 schema 語意，cllm schema 路徑無縫）。
inline constexpr const char *kStructTool = "__cllm_structured_output__";
inline constexpr int kDefaultMaxTokens = 4096;

// 送出：OpenAI 請求 → Anthropic 請求。
J to_anthropic(J &oai, int default_max_tokens = kDefaultMaxTokens);

// 收回（非串流）：Anthropic 回應 → OpenAI {"choices":[{"message":{…}}]}。
J from_anthropic(J &anth);

// Anthropic {"type":"error","error":{"message"}} → OpenAI {"error":{"message"}}。
J err_to_openai(J &anth_err);

// 串流：逐筆吃 Anthropic SSE 的 data JSON，吐 0..N 條要往 cllm 寫的 OpenAI SSE（含結尾空行）。
class StreamXlate {
public:
  std::vector<std::string> feed(J &data);  // Anthropic 事件 → OpenAI SSE data 行

private:
  std::set<int> struct_idx_;  // 哪些 content block index 是強制工具（→當 content 吐）
};

}  // namespace axl
