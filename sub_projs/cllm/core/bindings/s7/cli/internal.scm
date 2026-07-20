; internal.scm — CLI 共用的退出碼、環境變數鍵、檔案／字串／shell 小工具。
;   對齊 core-py 的 internal.py（＝C++ cli_internal.hpp）：退出碼常數／env 鍵／檔案讀取。
; 葉模組：不依賴任何姊妹 .scm，只用 s7 內建；作為其餘 CLI 模組的共用底（無環 DAG）。

; ── 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。──
(define EXIT-OK 0)
(define EXIT-USAGE 1)
(define EXIT-REQUEST 2)
(define EXIT-CANCEL 130)

(define CONFIG-ENV-VAR "LLM_CLI_CONFIG") ; 對齊 cli_internal.hpp 的 kConfigEnvVar

; 去掉字串尾端的 \r\n（stdin 收尾，避免多餘空白進 prompt）。
(define (rstrip-eol s)
  (let loop ((n (string-length s)))
    (if (and (> n 0)
             (let ((c (string-ref s (- n 1)))) (or (char=? c #\newline) (char=? c #\return))))
        (loop (- n 1))
        (substring s 0 n))))

; 整段讀一個輸入 port（read-char 累進；prompt/檔案量小，夠用）。
(define (slurp-port p)
  (let ((sb (open-output-string)))
    (let loop ((c (read-char p)))
      (if (eof-object? c) (get-output-string sb)
          (begin (write-char c sb) (loop (read-char p)))))))

; 整檔讀成字串（config／media 描述子等）。讀不到回 #f（由呼叫端轉退出碼）。
(define (read-file-string path)
  (catch #t
    (lambda () (call-with-input-file path slurp-port))
    (lambda args #f)))

; stdin 是否為互動終端（是＝別讀，避免卡住）。靠 shell `test -t 0`（子行程繼承 fd0）。
(define (stdin-tty?) (= 0 (system "test -t 0")))

; 跑 shell 指令並捕獲 stdout（s7 (system cmd #t)），去尾端換行。失敗回空字串。
(define (shell-capture cmd)
  (rstrip-eol (or (system cmd #t) "")))

; shell 單引號跳脫：把字串包成可安全塞進 '…' 的形式（' → '\''）。
(define (shq s)
  (let ((sb (open-output-string)))
    (write-char #\' sb)
    (for-each (lambda (c) (if (char=? c #\') (display "'\\''" sb) (write-char c sb)))
              (string->list s))
    (write-char #\' sb)
    (get-output-string sb)))

; 把內容寫進一個一次性 tmp 檔，回路徑（供 jq 剖析用）。
(define (write-tmp content)
  (let ((path (string-append "/tmp/cllm_s7_" (number->string (abs (random 100000000))) ".json")))
    (call-with-output-file path (lambda (p) (display content p)))
    path))

; 字串是否為合法 JSON（靠 jq -e .）。
(define (json-valid? str)
  (let* ((t (write-tmp str))
         (ok (= 0 (system (string-append "jq -e . " (shq t) " >/dev/null 2>&1")))))
    (system (string-append "rm -f " (shq t)))
    ok))

; 對 JSON 字串跑 jq filter，回捕獲結果（去尾端換行）。
(define (jq-on str filter)
  (let* ((t (write-tmp str))
         (out (shell-capture (string-append "jq " filter " " (shq t)))))
    (system (string-append "rm -f " (shq t)))
    out))
