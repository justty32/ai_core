#pragma once
// llm_reflect.hpp — cllm C++ 便利層的 glaze 反射糖：struct＝唯一真相源。
//
// 與 llm.hpp 刻意分檔：基本人體工學（聚合／expected／串流糖）零依賴；要「struct 進、struct 出」
// 才 include 本檔（需要 glaze 標頭——install-dev.sh 已複製進 prefix，pkg-config cllm 的 -I 就找得到）。
//
// 提供（cabi.hpp 檔頭預告的那層便利糖）：
//   · ask_as<T>(client, prompt)      結構化輸出：反射生 schema 約束模型 → 回應直接解回 T
//   · make_tool<Args>(name, desc)    從 struct Args 反射工具參數 schema → abi::ToolDef
//   · args_as<Args>(tool_call)       把模型回的 ToolCall.arguments（JSON 字串）解回 Args
//   · schema_of<T>／schema_for<T>／parse_as<T>  上面三者的組件，也可單獨用
//
// 編：g++ -std=c++23 x.cpp $(pkg-config --cflags --libs cllm)
// 用：
//   struct Character { std::string name; int affection{}; std::vector<std::string> lines; };
//   if (auto c = llm::ask_as<Character>(client, "生成一個傲嬌角色")) use(c->name);

#include "llm.hpp"

#include <glaze/glaze.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace llm {

// 生 schema 專用 opts。★★ error_on_missing_keys 一定要開（2026-07-14 真後端實測血淚）：
//   glz::write_json_schema<T>() 用預設 opts 生的 schema 只有 properties＋additionalProperties:false、
//   **沒有 required**——JSON Schema 語意下全部欄位變選填，後端約束解碼只保證「不吐多餘鍵」、
//   不保證「每欄都吐」（實測 LM Studio + gemma 對三欄 struct 只回一欄）。
//   開了之後：非 optional 欄位自動列入 required；std::optional<…> 欄位正確排除在外
//   （型別還自動變 ["…","null"]）。**別手動把 properties 全塞 required**——會錯標真正選填的欄位。
// ⚠ 這種缺陷離線 fixture 驗不出來（假回應欄位當然齊），只有打真後端才現形。
inline constexpr glz::opts kSchemaOpts{.error_on_missing_keys = true};

// 解析「模型回應」用的寬鬆 opts：多鍵不炸（error_on_unknown_keys=false）；缺鍵照樣報錯——
// 與 kSchemaOpts 生出的 required 對稱（模型該給的沒給＝真錯誤，別靜默吞）。
inline constexpr glz::opts kLenientOpts{.error_on_unknown_keys = false};

// 從 struct T 反射生 JSON Schema 字串。生成失敗回 "{}"（空 schema＝不約束）。
template <class T>
std::string schema_of() {
  auto s = glz::write_json_schema<T, kSchemaOpts>();
  return s ? std::move(*s) : std::string("{}");
}

// 包成 abi::Schema（塞 Request.schema 用）。
template <class T>
abi::Schema schema_for(std::string name = "response") {
  return abi::Schema{.name = std::move(name), .json = schema_of<T>()};
}

// JSON 字串 → T。解析失敗＝統一 Error（message 帶 glaze 錯誤定位，partial 帶原文）。
template <class T>
Result<T> parse_as(std::string_view json) {
  std::string buf(json); // glaze 要可平移的緩衝；順便給 format_error 定位用
  T value{};
  if (auto ec = glz::read<kLenientOpts>(value, buf))
    return std::unexpected(Error{.message = "JSON 解析失敗：" + glz::format_error(ec, buf),
                                 .partial = std::move(buf)});
  return value;
}

// 主糖：struct T 即唯一真相源——schema_of<T>() 約束模型 → 回應 parse_as<T>() 解回 T。
template <class T>
Result<T> ask_as(const Client &c, std::string_view prompt, std::string name = "response") {
  return c.ask(abi::Request{.prompt = std::string(prompt), .schema = schema_for<T>(std::move(name))})
      .and_then([](Reply &&r) { return parse_as<T>(r.text); });
}

// 從 struct Args 反射工具參數 schema，組成 ToolDef（送 Request.tools／ask_tools）。
template <class Args>
abi::ToolDef make_tool(std::string name, std::string description) {
  return abi::ToolDef{.name = std::move(name),
                      .description = std::move(description),
                      .parameters = schema_of<Args>()};
}

// 把模型回的 ToolCall.arguments 解回 Args——工具呼叫這端的型別安全補圓（舊版沒做的那半）。
template <class Args>
Result<Args> args_as(const abi::ToolCall &call) {
  return parse_as<Args>(call.arguments);
}

// 從 config struct 反射生 Modality（cabi.hpp 預告的 modality<Config>）：
// struct 欄位＝該模態的生成參數，glz::write_json 序列化成 config JSON（塞 Request.modalities）。
template <class Config>
abi::Modality modality(std::string name, const Config &cfg) {
  auto json = glz::write_json(cfg);
  return abi::Modality{.name = std::move(name),
                       .config = json ? std::move(*json) : std::string{}};
}

} // namespace llm
