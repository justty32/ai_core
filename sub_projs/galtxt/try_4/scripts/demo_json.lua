-- demo_json.lua — try_4 的 Lua 薄層結構化輸出示範：llm.ask_json 回原生 table。
--
-- schema 以 JSON Schema 字串提供（腳本沒有 C++ struct 可反射，走 runtime schema）；
-- C++ 送出約束模型、把回應 JSON 直接解成 Lua table 回來——腳本不碰 JSON，拿到就是原生 table。
-- 離線走 fake_json fixture（回罐頭 Character）。

local schema = [[
{"type":"object",
 "properties":{"name":{"type":"string"},
               "affection":{"type":"integer"},
               "lines":{"type":"array","items":{"type":"string"}}},
 "required":["name","affection","lines"]}
]]

local c = llm.ask_json{
  prompt   = "生成一個傲嬌女角色",
  schema   = schema,
  name     = "character",
  endpoint = FIXTURES .. "fake_json/chat/completions",
}
print(string.format("[lua] 結構化 => name=%s affection=%d lines[1]=%s",
                    c.name, c.affection, c.lines[1]))
