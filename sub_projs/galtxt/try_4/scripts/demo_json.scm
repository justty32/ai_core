; demo_json.scm — try_4 的 s7 薄層結構化輸出示範：llm-ask-json 回原生 alist。
;
; 同 demo_json.lua 的用意，換成 Scheme：C++ 把結構化回應解成 alist（(("name" . …) …)），
; 腳本用 assoc 導航、不碰 JSON。離線走 fake_json fixture。

(define schema
  "{\"type\":\"object\",
    \"properties\":{\"name\":{\"type\":\"string\"},
                    \"affection\":{\"type\":\"integer\"},
                    \"lines\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},
    \"required\":[\"name\",\"affection\",\"lines\"]}")

(define c (llm-ask-json "生成一個傲嬌女角色"
                        :schema schema
                        :name "character"
                        :endpoint (string-append *fixtures* "fake_json/chat/completions")))

(display (string-append "[s7] 結構化 => name=" (cdr (assoc "name" c))
                        " affection=" (number->string (cdr (assoc "affection" c)))
                        " lines[0]=" (car (cdr (assoc "lines" c)))))
(newline)
