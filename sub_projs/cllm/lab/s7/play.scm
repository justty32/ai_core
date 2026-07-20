; play.scm — 從 s7 用 cllm：基本、串流、tools+on-tool、media 輸出、schema+JSON(經 jq)、
;   media 輸入+modalities、shell 呼叫。
; 跑：source ~/dev/cllm/env.sh 後  llm-s7 play.scm "$CLLM_FIXTURES"
(define base (if (and (pair? *argv*) (string? (car *argv*))) (car *argv*) ""))
(define (ep n) (string-append base n))

; s7 無原生 json：需要抽欄位時一律 shell-out 給 jq（寫檔→跑 jq→讀回），全篇共用這支 slurp。
(define (slurp path)
  (call-with-input-file path (lambda (p) (let ((l (read-line p))) (if (eof-object? l) "" l)))))

(display (string-append "[s7] ask => " (llm-ask "你好" (ep "fake/chat/completions")))) (newline)

(display "[s7] 串流 => ")
(llm-ask "數到五" :endpoint (ep "fake_stream/chat/completions") :stream #t
         :on-delta (lambda (p) (display (string-append "[" p "]")) #f))
(newline)

; :tools + :on-tool — 模型要求呼叫工具（fake_tool fixture 固定回一個 get_weather 呼叫）。
; ⚠ 回呼只做無害的事：這裡只 set! 存值，JSON 解析（jq shell-out）挪到 llm-ask 呼叫「之後」再做，
;   不在回呼裡跑 system／檔案 I/O。
(define tool-name #f)
(define tool-args #f)
(llm-ask "東京天氣如何？" :endpoint (ep "fake_tool/chat/completions")
         :tools (list (list "get_weather" "查詢城市天氣"
                             "{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"},\"unit\":{\"type\":\"string\"}}}"))
         :on-tool (lambda (id name arguments)
                    (set! tool-name name)
                    (set! tool-args arguments)
                    #t)) ; 回 #t：只有一個 tool_call，示範中止語意
(call-with-output-file "/tmp/cllm_s7_tool.json" (lambda (p) (display tool-args p)))
(system "jq -r '.city' /tmp/cllm_s7_tool.json > /tmp/cllm_s7_tool_city.out")
(display (string-append "[s7] tool => " tool-name " " tool-args
                         " (city=" (slurp "/tmp/cllm_s7_tool_city.out") ")"))
(newline)

; fake_media fixture + :on-media — 模型產出媒體（audio）；回呼只做 display，無害。
(llm-ask "說句話" :endpoint (ep "fake_media/chat/completions")
         :on-media (lambda (mime bytes)
                     (display (string-append "[s7] media out => mime=" mime
                                              " bytes=" (number->string (string-length bytes))))
                     (newline)
                     #f))

; schema → JSON → jq 抽欄位（s7 無原生 json，順便示範 shell 呼叫）
(define raw (llm-ask "給我角色" :endpoint (ep "fake_json/chat/completions") :schema "{\"type\":\"object\"}"))
(call-with-output-file "/tmp/cllm_s7.json" (lambda (p) (display raw p)))
(system "jq -r '.name' /tmp/cllm_s7.json > /tmp/cllm_s7.out")
(display (string-append "[s7] json(jq) => name=" (slurp "/tmp/cllm_s7.out"))) (newline)

; :media（data URI 輸入）+ :modalities（想要的輸出模態＋參數）— fixture 忽略 body，只驗搬運不炸
(define multi (llm-ask "描述這張圖" :endpoint (ep "fake/chat/completions")
                        :media (list (list "data:image/png;base64,iVBORw0KGgo=" #f #f))
                        :modalities (list (cons "audio" "{\"voice\":\"alloy\",\"format\":\"wav\"}"))))
(display (string-append "[s7] media in+modality => " (if multi "ok" "fail"))) (newline)

; shell 呼叫 llm CLI
(system (string-append "printf '[s7] shell(llm) => '; llm 你好 --endpoint '" (ep "fake/chat/completions") "'"))
(newline)
