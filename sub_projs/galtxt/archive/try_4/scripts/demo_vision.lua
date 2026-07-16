-- demo_vision.lua — try_4 的 Lua 薄層多媒體示範：llm.ask_vision 帶文字＋圖片、回字串。
--
-- images 每格：字串＝外部 URL（後端自己抓）；或 {file=路徑, mime=…}＝讀本地檔轉 base64 data URI。
-- 離線走 fake fixture（回罐頭答覆，不真看圖）。真用途換成真 endpoint＋真圖 URL/檔即可。

local answer = llm.ask_vision{
  prompt   = "這張圖是什麼？",
  endpoint = FIXTURES .. "fake/chat/completions",
  images   = { "https://example.com/cat.png" },
}
print("[lua] vision => " .. answer)
