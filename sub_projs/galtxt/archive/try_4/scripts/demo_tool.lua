-- demo_tool.lua — try_4 的 Lua 薄層工具呼叫示範：llm.ask_tools 回 {name, arguments=table}。
--
-- 工具的參數 schema 以 JSON Schema 字串提供；模型回的 tool_calls，其 arguments（模型產生的 JSON）
-- 由 C++ 直接解成 Lua table 回來。離線走 fake_tool fixture（回罐頭 get_weather 呼叫）。

local schema = [[
{"type":"object",
 "properties":{"city":{"type":"string"},"unit":{"type":"string"}},
 "required":["city","unit"]}
]]

local calls = llm.ask_tools{
  prompt   = "東京天氣如何？",
  endpoint = FIXTURES .. "fake_tool/chat/completions",
  tools = {
    { name = "get_weather", description = "查詢某城市天氣", schema = schema },
  },
}

print("[lua] 模型要求呼叫 " .. #calls .. " 個工具")
for _, c in ipairs(calls) do
  print(string.format("[lua]   %s(city=%s, unit=%s)",
                      c.name, c.arguments.city, c.arguments.unit))
end
