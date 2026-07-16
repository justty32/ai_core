// llm_tool.cpp — llm_tool.hpp 的實作。
//
// 送出：messages ＋ tools[]（每個 tool＝{type:"function", function:{name,description,parameters}}）。
//   parameters 是 schema「物件」不是字串，故用 glz::raw_json 讓 glaze 原樣嵌入（不當字串加引號）。
// 收回：choices[0].message.tool_calls[]，各含 function.name＋function.arguments（JSON 字串）。

#include "llm_tool.hpp"

#include <optional>
#include <vector>

#include <glaze/glaze.hpp>

namespace llm {
// 具名 namespace（非匿名）：避免與 llm.cpp／llm_media.cpp 同名 struct 的 glaze COMDAT section
// 跨 TU 撞名（GCC 匿名 namespace 一律 mangle 成 _GLOBAL__N_1）。
namespace tool_impl {

// ── 送出的請求體 ──
struct ReqMessage { std::string role; std::string content; };
struct ReqFn      { std::string name; std::string description; glz::raw_json parameters; };
struct ReqTool    { std::string type = "function"; ReqFn function; };
struct ReqBody {
    std::optional<std::string> model;
    std::vector<ReqMessage> messages;
    std::vector<ReqTool> tools;
    // 取樣屬性（apply_sampling 灌入，未設略過）
    std::optional<float> temperature;
    std::optional<float> top_p;
    std::optional<int>   max_tokens;
    std::optional<float> presence_penalty;
    std::optional<float> frequency_penalty;
    std::optional<int>   seed;
};

// ── 收回的回應體（只挑 tool_calls 需要的） ──
struct RespFn        { std::string name; std::string arguments; };
struct RespToolCall  { std::string id; RespFn function; };
struct RespMessage   { std::vector<RespToolCall> tool_calls; };
struct RespChoice    { RespMessage message; };
struct RespBody      { std::vector<RespChoice> choices; };

constexpr glz::opts kLenient{ .error_on_unknown_keys = false };

}  // namespace tool_impl
using namespace tool_impl;

std::vector<ToolCall> ask_tools(const Client& client,
                                std::string_view prompt,
                                const std::vector<Tool>& tools) {
    // 組請求：把每個 Tool 攤成 OpenAI 的 {type,function{name,description,parameters}}。
    ReqBody payload;
    payload.messages = { { "user", std::string(prompt) } };
    for (const auto& t : tools) {
        ReqTool rt;
        rt.function.name = t.name;
        rt.function.description = t.description;
        rt.function.parameters = glz::raw_json{ t.parameters };  // schema 原樣嵌入（物件，非字串）
        payload.tools.push_back(std::move(rt));
    }
    apply_sampling(client, payload);   // 注意：ReqBody 無 stream 欄位（工具呼叫先做非串流）

    auto body = glz::write_json(payload);
    if (!body) return {};

    // 送出（共用 llm::post）→ 反射解析 tool_calls
    std::string raw = post(client, *body);
    RespBody parsed{};
    if (glz::read<kLenient>(parsed, raw) || parsed.choices.empty()) return {};

    std::vector<ToolCall> calls;
    for (const auto& tc : parsed.choices[0].message.tool_calls) {
        calls.push_back(ToolCall{ tc.id, tc.function.name, tc.function.arguments });
    }
    return calls;
}

}  // namespace llm
