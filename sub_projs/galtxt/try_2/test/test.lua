-- test.lua — 全離線：curl file:// 灌假回應，驗 request 組裝 + 回應解析
-- 跑：host.exe test/test.lua（從 try_2 根跑；路徑自解，不寫死機器路徑）
local HERE = debug.getinfo(1, "S").source:sub(2):match("^(.*[/\\])") or ""
local llm = dofile(HERE .. "../src/llm.lua")
local P = dofile(HERE .. "../src/_path.lua")
llm.base_url = P.fixture("fake")

io.write("=== 呼叫 llm_entry ===\n")
local r = llm.llm_entry{
  prompt = "你是一隻貓娘，請回答我問題",
  sys = "你是傲嬌貓娘",
  temp = 0.8, top_p = 0.9, max_tokens = 128,
}
io.write("content => " .. tostring(r) .. "\n")

io.write("=== 送出的請求 body（llm.last_request）===\n")
io.write(llm.last_request .. "\n")

-- 邊界：未知參數要被擋
io.write("=== 未知參數應報錯 ===\n")
local ok, err = pcall(llm.llm_entry, { prompt = "x", nonsense = 1 })
io.write((ok and "✗ 沒擋住" or ("✓ 擋住：" .. tostring(err))) .. "\n")

-- 邊界：缺 prompt/infile 要報錯
local ok2, err2 = pcall(llm.llm_entry, { temp = 0.5 })
io.write((ok2 and "✗ 沒擋住" or ("✓ 擋住：" .. tostring(err2))) .. "\n")

-- Lua 格式輸出：模型吐 Lua table 字面量 → sandbox load → 原生 table
io.write("=== Lua 格式輸出（--lua / lua=true）===\n")
llm.base_url = P.fixture("fake_lua")
local ch = llm.llm_entry{ prompt = "生成角色", lua = true }
io.write(("解析型別=%s name=%s affection+1=%s 台詞1=%s\n"):format(
  type(ch), tostring(ch and ch.name), tostring(ch and (ch.affection + 1)), tostring(ch and ch.lines[1])))

-- 沙箱：空環境沒有 os，副作用型內容不該能跑
local evil, eerr = llm.parse_lua("os.exit(99)")
io.write((evil == nil and ("✓ 沙箱擋住副作用（" .. tostring(eerr) .. "）") or "✗ 沙箱破了") .. "\n")

-- 串流：callback 持續被餵、UTF-8 不切半、拼回＝完整內容、回傳 {ok=true}
io.write("=== 串流（streaming=true + callback，chunk_chars=3）===\n")
llm.base_url = P.fixture("fake_stream")
local calls, parts, bad = 0, {}, false
local sr = llm.ask{
  prompt = "hi", streaming = true, chunk_chars = 3,
  callback = function(chunk)
    calls = calls + 1
    parts[#parts + 1] = chunk
    if utf8.len(chunk) == nil then bad = true end        -- 切到半個字就會是 nil
  end,
}
local full = table.concat(parts)
local expect = "喵～主人好壞，人家才不是為你回答的"
io.write(("ok=%s calls=%d 拼回正確=%s 無半字=%s\n"):format(
  tostring(sr.ok), calls, tostring(full == expect), tostring(not bad)))
os.exit(0)
