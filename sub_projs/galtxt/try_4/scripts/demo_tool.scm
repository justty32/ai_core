; demo_tool.scm — try_4 的 s7 薄層工具呼叫示範：llm-ask-tools 回 (alist …)。
;
; 同 demo_tool.lua 的用意，換成 Scheme：tools＝(list (list name desc schema) …)；
; 回來每個工具呼叫是 alist（(("name" . …) ("arguments" . alist))），arguments 也已由 C++ 解成 alist。
; 離線走 fake_tool fixture。

(define schema
  "{\"type\":\"object\",
    \"properties\":{\"city\":{\"type\":\"string\"},\"unit\":{\"type\":\"string\"}},
    \"required\":[\"city\",\"unit\"]}")

(define calls (llm-ask-tools "東京天氣如何？"
                             :tools (list (list "get_weather" "查詢某城市天氣" schema))
                             :endpoint (string-append *fixtures* "fake_tool/chat/completions")))

(display (string-append "[s7] 模型要求呼叫 " (number->string (length calls)) " 個工具"))
(newline)
(for-each
  (lambda (c)
    (let ((name (cdr (assoc "name" c)))
          (args (cdr (assoc "arguments" c))))
      (display (string-append "[s7]   " name "(city=" (cdr (assoc "city" args))
                              ", unit=" (cdr (assoc "unit" args)) ")"))
      (newline)))
  calls)
