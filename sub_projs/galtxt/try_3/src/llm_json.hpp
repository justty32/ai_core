// llm_json.hpp — galtxt try_3：LLM 結構化輸出（structured output，丟 JSON Schema 約束回應）。
//
// 在 llm::Client 之上長「丟一個 struct 進去、拿一個 struct 出來」。這是本線主張的最漂亮一環——
// ★ 目標型別 T 就是唯一真相源：glz::write_json_schema<T> 反射生成 schema，塞進
//   response_format.json_schema 送出（約束模型只能回符合的 JSON）；回來的 JSON 再
//   glz::read_json 反射回 T。全程零手寫 schema、零手寫解析。
//
// ★ schema 走 schema_of<T>()（＝kSchemaOpts，開 error_on_missing_keys）而非裸 write_json_schema——
//   否則生出的 schema 沒有 `required`，模型可以只吐部分欄位。血淚實測與原理見 llm_schema.hpp 檔頭。

#pragma once
#include <optional>
#include <string>
#include <string_view>

#include <glaze/glaze.hpp>   // read_json：把回應反射回 T
#include "llm.hpp"
#include "llm_schema.hpp"    // schema_of<T>()：反射生回應 schema（含 required）

namespace llm {

// 非模板部分（真正發請求）：把 schema（字串）塞進 response_format 送出，回原始回應 content
// （一段被約束成符合 schema 的 JSON 字串）。schema_name＝給這個 schema 的識別名。
std::string ask_json_raw(const Client& client,
                         std::string_view prompt,
                         std::string_view schema_name,
                         std::string_view schema_json);

// ★ 結構化輸出：丟 struct T 進去，回 std::optional<T>。
//   T＝唯一真相源：schema_of<T>() 生成 schema 約束模型、read_json 把回應反射回 T。
//   傳輸失敗會 throw（來自 post）；回應解析失敗＝回 nullopt。
template <class T>
std::optional<T> ask_as(const Client& client,
                        std::string_view prompt,
                        std::string_view schema_name = "response") {
    std::string content = ask_json_raw(client, prompt, schema_name, schema_of<T>());
    T value{};
    if (glz::read_json(value, content)) return std::nullopt;   // 回應解不成 T
    return value;
}

}  // namespace llm
