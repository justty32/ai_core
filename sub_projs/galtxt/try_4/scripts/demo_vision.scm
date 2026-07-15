; demo_vision.scm — try_4 的 s7 薄層多媒體示範：llm-ask-vision 帶文字＋圖片、回字串。
;
; images 每格：字串＝外部 URL；或 (list "file" 路徑 mime)＝讀本地檔轉 base64 data URI。
; 離線走 fake fixture（回罐頭答覆）。真用途換真 endpoint＋真圖即可。

(define answer (llm-ask-vision "這張圖是什麼？"
                               :images (list "https://example.com/cat.png")
                               :endpoint (string-append *fixtures* "fake/chat/completions")))
(display (string-append "[s7] vision => " answer))
(newline)
