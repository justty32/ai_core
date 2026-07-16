; example.scm — 從 s7 用 cllm：基本、串流、schema+JSON(經 jq)、shell 呼叫。
; 跑：source ~/repo/dev/env.sh 後  llm-s7 example.scm "$CLLM_FIXTURES"
(define base (if (and (pair? *argv*) (string? (car *argv*))) (car *argv*) ""))
(define (ep n) (string-append base n))

(display (string-append "[s7] ask => " (llm-ask "你好" (ep "fake/chat/completions")))) (newline)

(display "[s7] 串流 => ")
(llm-ask "數到五" :endpoint (ep "fake_stream/chat/completions") :stream #t
         :on-delta (lambda (p) (display (string-append "[" p "]")) #f))
(newline)

; schema → JSON → jq 抽欄位（s7 無原生 json，順便示範 shell 呼叫）
(define raw (llm-ask "給我角色" :endpoint (ep "fake_json/chat/completions") :schema "{\"type\":\"object\"}"))
(define (slurp path)
  (call-with-input-file path (lambda (p) (let ((l (read-line p))) (if (eof-object? l) "" l)))))
(call-with-output-file "/tmp/cllm_s7.json" (lambda (p) (display raw p)))
(system "jq -r '.name' /tmp/cllm_s7.json > /tmp/cllm_s7.out")
(display (string-append "[s7] json(jq) => name=" (slurp "/tmp/cllm_s7.out"))) (newline)

; shell 呼叫 llm CLI
(system (string-append "printf '[s7] shell(llm) => '; llm 你好 --endpoint '" (ep "fake/chat/completions") "'"))
(newline)
