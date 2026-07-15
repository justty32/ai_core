; demo_stream.scm — try_4 的 s7 薄層串流示範：:keyword 選項帶 :on-delta 回呼。
;
; 同 demo_stream.lua 的用意，換成 Scheme：C++ 逐段回呼進 s7 的 on-delta，每段用 [] 框起印出；
; 回呼回 #f＝繼續（回 #t 可中止）。(llm-ask …) 仍回完整答案。離線走 fake_stream fixture。

(display "[s7] 串流逐段 => ")
(define whole
  (llm-ask "你好"
           :endpoint (string-append *fixtures* "fake_stream/chat/completions")
           :stream #t
           :on-delta (lambda (piece)
                       (display (string-append "[" piece "]"))
                       #f)))            ; 回 #t 可中止
(display (string-append "　合＝" whole))
(newline)
