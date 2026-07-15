-- demo_stream.lua — try_4 的 Lua 薄層串流示範：table 形式帶 on_delta 回呼。
--
-- 串流：C++ 核心逐段（SSE delta）回呼進 Lua 的 on_delta，這裡把每段用 [] 框起即時印出；
-- 回呼回 false＝繼續（回 true 可中止）。ask 仍回「完整組合後的答案」。
-- 離線走 fake_stream fixture（多段 SSE），不必開後端。

io.write("[lua] 串流逐段 => ")
local whole = llm.ask{
  prompt   = "你好",
  endpoint = FIXTURES .. "fake_stream/chat/completions",
  stream   = true,
  on_delta = function(piece)
    io.write("[" .. piece .. "]")
    return false   -- 回 true 可中止
  end,
}
print("　合＝" .. whole)
