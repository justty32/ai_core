-- example.lua — cllm Lua binding：基本 ask、串流、schema+JSON(dkjson)、tools、media、
--   modalities、shell(CLI) 呼叫。
-- 跑：source ~/dev/env.sh 後  lua example.lua "$CLLM_FIXTURES"
local llm = require("llm")
local json = require("dkjson")
local base = arg[1] or ""
local function ep(n) return base .. n end

print("[lua] ask => " .. llm.ask("你好", ep("fake/chat/completions")))
io.write("[lua] 串流 => ")
llm.ask{ prompt="數到五", endpoint=ep("fake_stream/chat/completions"), stream=true,
         on_delta=function(p) io.write("["..p.."]") return false end }
print()

-- schema → JSON → dkjson 解析
local raw = llm.ask{ prompt="給我角色", endpoint=ep("fake_json/chat/completions"), schema='{"type":"object"}' }
local o = json.decode(raw)
print(("[lua] json => name=%s affection=%s lines=%d"):format(o.name, tostring(o.affection), #o.lines))

-- tools + on_tool：fake_tool fixture 固定回一個 tool_call；arguments 是 JSON 字串，用 dkjson 解
llm.ask{ prompt="東京天氣如何？", endpoint=ep("fake_tool/chat/completions"),
         tools={ { name="get_weather", description="查詢城市天氣",
                   parameters='{"type":"object","properties":{"city":{"type":"string"},"unit":{"type":"string"}}}' } },
         on_tool=function(call)
           local args = json.decode(call.arguments)
           print(("[lua] tool => %s(city=%s, unit=%s)"):format(call.name, args.city, args.unit))
           return false
         end }

-- media 輸出：fake_media fixture 回 audio → on_media 收 mime/bytes
llm.ask{ prompt="說句話", endpoint=ep("fake_media/chat/completions"),
         on_media=function(m)
           print(("[lua] media out => mime=%s bytes=%d"):format(m.mime, #m.bytes))
           return false
         end }

-- media 輸入＋modalities：掛 data URI 圖片＋audio 模態參數打 fake（body 被忽略，驗證搬運不炸）
local ok, err = llm.ask{ prompt="描述這張圖", endpoint=ep("fake/chat/completions"),
         media={ { url="data:image/png;base64,iVBORw0KGgo=" } },
         modalities={ { name="audio", config='{"voice":"alloy"}' } } }
print("[lua] media in+modality => " .. (ok and "ok" or tostring(err)))

-- shell 呼叫：從 Lua 呼叫 llm CLI
local h = io.popen("llm 你好 --endpoint '"..ep("fake/chat/completions").."'")
print("[lua] shell(llm) => " .. (h:read("*a") or ""):gsub("%s+$",""))
h:close()
