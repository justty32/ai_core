// llm_json.cpp — llm_json.hpp 的實作（結構化輸出的非模板部分：發請求＋取 content）。
//
// 送出：response_format = {type:"json_schema", json_schema:{name, strict, schema}}。
//   schema 是「物件」不是字串，故用 glz::raw_json 讓 glaze 原樣嵌入。
// 收回：choices[0].message.content——結構化輸出把 JSON 放在 content 裡當文字（一段 JSON 字串）。

#include "llm_json.hpp"

#include <optional>
#include <vector>

#include <glaze/glaze.hpp>

namespace llm {
// 具名 namespace（非匿名）：避免與其它 llm_*.cpp 同名 struct 的 glaze COMDAT section
// 跨 TU 撞名（GCC 匿名 namespace 一律 mangle 成 _GLOBAL__N_1）。
namespace json_impl {

// ── 送出的請求體 ──
struct ReqMessage { std::string role; std::string content; };
struct JsonSchema {
    std::string name;
    bool strict = true;
    glz::raw_json schema;   // schema 物件原樣嵌入
};
struct ResponseFormat {
    std::string type = "json_schema";
    JsonSchema json_schema;
};
struct ReqBody {
    std::optional<std::string> model;
    std::vector<ReqMessage> messages;
    ResponseFormat response_format;
    // 取樣屬性（apply_sampling 灌入，未設略過）
    std::optional<float> temperature;
    std::optional<float> top_p;
    std::optional<int>   max_tokens;
    std::optional<float> presence_penalty;
    std::optional<float> frequency_penalty;
    std::optional<int>   seed;
};

// ── 收回：結構化輸出的 JSON 就在 content 裡（字串） ──
struct RespMessage { std::string content; };
struct RespChoice  { RespMessage message; };
struct RespBody    { std::vector<RespChoice> choices; };

constexpr glz::opts kLenient{ .error_on_unknown_keys = false };

}  // namespace json_impl
using namespace json_impl;

std::string ask_json_raw(const Client& client,
                         std::string_view prompt,
                         std::string_view schema_name,
                         std::string_view schema_json) {
    ReqBody payload;
    payload.messages = { { "user", std::string(prompt) } };
    payload.response_format.json_schema.name = std::string(schema_name);
    payload.response_format.json_schema.schema = glz::raw_json{ std::string(schema_json) };
    apply_sampling(client, payload);

    auto body = glz::write_json(payload);
    if (!body) return {};

    std::string raw = post(client, *body);   // 共用 llm::post
    RespBody parsed{};
    if (glz::read<kLenient>(parsed, raw) || parsed.choices.empty()) return {};
    return parsed.choices[0].message.content;
}

}  // namespace llm
