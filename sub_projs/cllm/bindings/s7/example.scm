; example.scm — cllm s7 binding 示範。
; 跑法（build.sh 會編出 llm-s7）：
;   ./llm-s7 example.scm "file:///<cllm絕對路徑>/test/fixtures/"   ; 離線走 fixture
;   ./llm-s7 example.scm                                           ; 無參數 → 內建 localhost（要真後端）
; endpoint 基底走 host 注入的 *argv*（第一個參數）。

(define base (if (and (pair? *argv*) (string? (car *argv*))) (car *argv*) ""))

; ① 位置形式：prompt + endpoint（base 為 "" 時省略 endpoint → 內建 localhost）
(define ans
  (if (> (string-length base) 0)
      (llm-ask "你好" (string-append base "fake/chat/completions"))
      (llm-ask "你好")))
(display (string-append "[s7] llm-ask => " ans))
(newline)

; ② keyword 形式 + 串流 :on-delta（逐段印；回 #f 續、#t 中止）
(when (> (string-length base) 0)
  (display "[s7] 串流逐段 => ")
  (define whole
    (llm-ask "數到五"
             :endpoint (string-append base "fake_stream/chat/completions")
             :stream #t
             :on-delta (lambda (piece)
                         (display (string-append "[" piece "]"))
                         #f)))
  (display (string-append "　合＝" whole))
  (newline))
