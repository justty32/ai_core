; demo.scm — try_4 的 s7 薄層示範：呼叫 C++ 綁定的 llm-ask。
;
; 同 demo.lua 的用意，換成 Scheme：這層極薄，一句 (llm-ask …) 把重活丟給 C++ 核心。
; 離線：用 host 注入的 *fixtures*（file://…/try_3/test/fixtures/）指向假回應。
; 真用途：省略 endpoint 參數即走 from_env()（預設本機 LM Studio）。

(define answer (llm-ask "你好" (string-append *fixtures* "fake/chat/completions")))
(display (string-append "[s7] llm-ask => " answer))
(newline)
