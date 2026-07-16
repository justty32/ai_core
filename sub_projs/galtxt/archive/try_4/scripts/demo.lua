-- demo.lua — try_4 的 Lua 薄層示範：呼叫 C++ 綁定的 llm.ask。
--
-- 這就是 try_4 想要的樣子：Lua 這層極薄，不碰 JSON／HTTP／schema——一句 llm.ask 把重活丟給 C++。
-- 離線：用 host 注入的 FIXTURES（file://…/try_3/test/fixtures/）指向假回應，不必開後端就能跑。
-- 真用途：把第二個參數（endpoint）拿掉即走 from_env()（預設本機 LM Studio），或自帶真 endpoint。

local answer = llm.ask("你好", FIXTURES .. "fake/chat/completions")
print("[lua] llm.ask => " .. answer)
