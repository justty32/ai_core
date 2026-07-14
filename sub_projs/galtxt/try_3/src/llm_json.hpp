// llm_json.hpp — galtxt try_3：LLM 結構化輸出（structured output，丟 JSON Schema 約束回應）。
//
// 在 llm::Client 之上長「丟一個 struct 進去、拿一個 struct 出來」。這是本線主張的最漂亮一環——
// ★ 目標型別 T 就是唯一真相源：glz::write_json_schema<T> 反射生成 schema，塞進
//   response_format.json_schema 送出（約束模型只能回符合的 JSON）；回來的 JSON 再
//   glz::read_json 反射回 T。全程零手寫 schema、零手寫解析。
//
// ⚠ OpenAI「strict」結構化輸出對 schema 有子集要求（所有欄位列入 required、additionalProperties:false…）。
//   glaze 生成的 schema 已含 additionalProperties:false；required 視後端寬嚴而定，本地伺服器多半不挑。
//   離線 fixture 不驗 schema，真後端 strict 模式若挑剔再按其規格微調 T 或 schema。

#pragma once
#include <optional>
#include <string>
#include <string_view>

#include <glaze/glaze.hpp>   // write_json_schema<T>／read_json：反射生成 schema、反射解析
#include "llm.hpp"

namespace llm {

// 非模板部分（真正發請求）：把 schema（字串）塞進 response_format 送出，回原始回應 content
// （一段被約束成符合 schema 的 JSON 字串）。schema_name＝給這個 schema 的識別名。
std::string ask_json_raw(const Client& client,
                         std::string_view prompt,
                         std::string_view schema_name,
                         std::string_view schema_json);

// ★ 結構化輸出：丟 struct T 進去，回 std::optional<T>。
//   T＝唯一真相源：write_json_schema<T> 生成 schema 約束模型、read_json 把回應反射回 T。
//   傳輸失敗會 throw（來自 post）；schema 生成或回應解析失敗＝回 nullopt。
template <class T>
std::optional<T> ask_as(const Client& client,
                        std::string_view prompt,
                        std::string_view schema_name = "response") {
    auto schema = glz::write_json_schema<T>();
    if (!schema) return std::nullopt;
    std::string content = ask_json_raw(client, prompt, schema_name, *schema);
    T value{};
    if (glz::read_json(value, content)) return std::nullopt;   // 回應解不成 T
    return value;
}

}  // namespace llm
