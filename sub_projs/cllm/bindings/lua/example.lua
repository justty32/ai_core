-- example.lua — cllm Lua binding：基本 ask、串流、schema+JSON(dkjson)、shell(CLI) 呼叫。
-- 跑：source ~/repo/dev/env.sh 後  lua example.lua "$CLLM_FIXTURES"
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

-- shell 呼叫：從 Lua 呼叫 llm CLI
local h = io.popen("llm 你好 --endpoint '"..ep("fake/chat/completions").."'")
print("[lua] shell(llm) => " .. (h:read("*a") or ""):gsub("%s+$",""))
h:close()
