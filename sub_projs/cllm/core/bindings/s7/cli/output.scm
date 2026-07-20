; output.scm — 四個出口回呼打包成 Sink（對齊 core-py output.py＝C++ cli_output.cpp 的 Sink）。
;   文字吐 stdout、tool_calls 一行一則 JSON、產出媒體落檔 --media-out、錯誤吐 stderr。
;   ⚠ binding 回呼安全鐵則：on-delta/on-tool/on-media/on-error 於 llm_ask（C++）執行途中被呼叫，
;     回呼內不可觸發 s7 error（longjmp 穿 C++ 堆疊＝UB）。故：on-delta 只 display（無害）即時吐；
;     on-tool/on-media/on-error 只「存值」入緩衝，jq／落檔等有風險的動作全挪到 llm-ask 回來後
;     sink-flush! 才做（對齊 example.scm 的作法）。這是 s7 相對 core-py 的優雅降級：
;     tool_calls／媒體改「收齊後一次吐」而非串流即時吐。

(define (make-sink media-out-dir)
  (let ((s (make-hash-table)))
    (hash-table-set! s 'media-out-dir media-out-dir)
    (hash-table-set! s 'wrote-text #f)
    (hash-table-set! s 'had-error #f)
    (hash-table-set! s 'media-err #f)
    (hash-table-set! s 'error-msg "")
    (hash-table-set! s 'media-n 0)   ; 已落檔媒體數（供編號檔名）
    (hash-table-set! s 'tools '())   ; 緩衝：(id name args) 逆序
    (hash-table-set! s 'media '())   ; 緩衝：(mime bytes) 逆序
    s))

; 文字：即時吐 stdout（display 無害、不會拋 s7 error）。串流／非串流都走這裡。
(define (sink-on-delta s)
  (lambda (piece)
    (display piece)
    (flush-output-port (current-output-port))
    (hash-table-set! s 'wrote-text #t)
    #f))

; 工具呼叫：只存值（延後到 flush 才 jq 組 JSON 吐）。
(define (sink-on-tool s)
  (lambda (id name args)
    (hash-table-set! s 'tools (cons (list id name args) (hash-table-ref s 'tools)))
    #f))

; 產出媒體：只存值（延後到 flush 才落檔）。
(define (sink-on-media s)
  (lambda (mime bytes)
    (hash-table-set! s 'media (cons (list mime bytes) (hash-table-ref s 'media)))
    #f))

; 錯誤：只設旗標＋存訊息（延後到 flush 才印 stderr）。
(define (sink-on-error s)
  (lambda (msg)
    (hash-table-set! s 'had-error #t)
    (hash-table-set! s 'error-msg msg)
    #f))

; llm-ask 回來後統一收尾：吐 tool_calls（jq）、落檔媒體、印錯誤。
(define (sink-flush! s)
  (for-each (lambda (t) (emit-tool-call s (car t) (cadr t) (caddr t)))
            (reverse (hash-table-ref s 'tools)))
  (for-each (lambda (m) (emit-media s (car m) (cadr m)))
            (reverse (hash-table-ref s 'media)))
  (when (hash-table-ref s 'had-error)
    (format (current-error-port) "請求失敗：~A~%" (hash-table-ref s 'error-msg))))

; 一則 tool_call → 一行 JSON {"tool","id","arguments"}（arguments 內嵌為物件；壞 JSON 退回字串）。
(define (emit-tool-call s id name args)
  (let ((tmp (string-append "/tmp/cllm_s7_tc_" (number->string (abs (random 1000000))) ".json")))
    (call-with-output-file tmp (lambda (p) (display (if (> (string-length args) 0) args "{}") p)))
    (let ((line
           (if (= 0 (system (string-append "jq -e . " (shq tmp) " >/dev/null 2>&1")))
               (shell-capture (string-append
                 "jq -cn --arg tool " (shq name) " --arg id " (shq id)
                 " --slurpfile a " (shq tmp)
                 " '{tool:$tool,id:$id,arguments:$a[0]}'"))
               (shell-capture (string-append ; 壞 JSON：arguments 原樣塞字串
                 "jq -cn --arg tool " (shq name) " --arg id " (shq id)
                 " --arg arguments " (shq args)
                 " '{tool:$tool,id:$id,arguments:$arguments}'")))))
      (system (string-append "rm -f " (shq tmp)))
      (display line) (newline)
      (flush-output-port (current-output-port)))))

; 一則產出媒體 → 落檔 --media-out（llm-media-N.<ext>，路徑吐 stdout）；沒目錄＝丟棄。
(define (emit-media s mime bytes)
  (let ((dir (hash-table-ref s 'media-out-dir)))
    (if (not dir)
        (format (current-error-port) "收到產出媒體（~A，~D bytes）但沒給 --media-out，已丟棄~%"
                mime (string-length bytes))
        (begin
          (hash-table-set! s 'media-n (+ 1 (hash-table-ref s 'media-n)))
          (let ((path (string-append dir "/llm-media-"
                                     (number->string (hash-table-ref s 'media-n))
                                     "." (ext-of mime))))
            (catch #t
              (lambda ()
                (call-with-output-file path (lambda (p) (display bytes p)))
                (display path) (newline)
                (flush-output-port (current-output-port)))
              (lambda args
                (format (current-error-port) "媒體落檔失敗：~A~%" path)
                (hash-table-set! s 'media-err #t))))))))
