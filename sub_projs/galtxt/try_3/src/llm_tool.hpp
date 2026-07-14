// llm_tool.hpp — galtxt try_3：LLM 工具呼叫（function calling）。
//
// 在 llm::Client 之上長「帶工具集發問、收模型要求呼叫的工具」。延續本線主張——
// ★ 工具的參數契約也「struct＝唯一真相源」：用 make_tool<Args>() 從一個 C++ struct
//   反射生成參數的 JSON Schema（glz::write_json_schema），不手寫 schema。
//
// 流程（OpenAI 相容）：送 tools[] → 模型回 message.tool_calls[]（函式名＋arguments JSON 字串）
//   → 你照函式名執行、把結果以 role:"tool" 訊息回送（多輪；本檔先做「拿到 tool_calls」這一段）。

#pragma once
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>   // read_json：把模型回的 arguments 反射解回 Args struct
#include "llm.hpp"
#include "llm_schema.hpp"    // schema_of<Args>()：反射生參數 schema（含 required）

namespace llm {

// 一個工具定義：函式名、給模型看的說明、參數的 JSON Schema（字串形式）。
struct Tool {
    std::string name;
    std::string description;
    std::string parameters;   // JSON Schema 字串；建議用 make_tool<Args>() 反射生成
};

// ★ 從 C++ struct 反射生成參數 JSON Schema —— 工具定義的「唯一真相源」就是這個 struct。
//   用法：  struct GetWeather { std::string city; std::string unit; };
//          llm::Tool t = llm::make_tool<GetWeather>("get_weather", "查詢某城市天氣");
//   ★ 走 schema_of<>（kSchemaOpts）而非裸 write_json_schema——才會生 `required`，否則模型可以
//     少給參數（Args 的欄位在 schema 裡全成選填）。理由詳見 llm_schema.hpp 檔頭。
template <class Args>
Tool make_tool(std::string name, std::string description) {
    return Tool{ std::move(name), std::move(description), schema_of<Args>() };
}

// 模型要求呼叫的一次工具：id、函式名、arguments（模型產生的 JSON 字串，照 Tool.parameters）。
//   拿到後用 glz::read_json(your_args_struct, tc.arguments) 反射解回你的 Args struct。
struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;
};

// 帶著工具集發問。回模型要求的 tool_calls（可能為空＝模型直接答了、沒要呼叫工具）。
// 非串流（工具呼叫通常一次收完）。傳輸失敗會 throw。
std::vector<ToolCall> ask_tools(const Client& client,
                                std::string_view prompt,
                                const std::vector<Tool>& tools);

}  // namespace llm
