-- demo_stream.lua — 展示串流：streaming=true + callback，內容一片片經 callback 餵回（離線 SSE 假回應）
-- 跑：host.exe demo_stream.lua
local HERE = debug.getinfo(1, "S").source:sub(2):match("^(.*[/\\])") or ""
local llm = dofile(HERE .. "../src/llm.lua")
llm.base_url = dofile(HERE .. "../src/_path.lua").fixture("fake_stream")

io.write("=== 串流（chunk_chars=3：每 3 個 UTF-8 字呼叫一次 callback）===\n")
local calls, full = 0, {}
local r = llm.ask{
  prompt = "打個招呼",
  streaming = true,
  chunk_chars = 3,
  callback = function(chunk)
    calls = calls + 1
    full[#full + 1] = chunk
    io.write(("[回呼#%d] «%s»（%d 字）\n"):format(calls, chunk, utf8.len(chunk)))
  end,
}
io.write(("結果 = { ok = %s }；callback 共 %d 次；拼回＝%s\n"):format(
  tostring(r.ok), calls, table.concat(full)))
