; config.scm — 三層 config 來源前二層（對齊 core-py config.py＝C++ cli_config.cpp 的 load_into）。
;   來源優先序（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。本檔只處理「config 檔」層；
;   旗標覆寫在 cli.scm。路徑：--config ＞ 環境變數 ＞ ~/.config/llm/config.json。
;   s7 無原生 JSON → config 檔靠 jq 剖析（to_entries 一次吐 key\tvalue，濾出已知鍵）。
;   未列鍵靜默忽略（lenient；與 C++ glaze 嚴格解析刻意分歧，見 README 已知落差）。

; 已知 config 鍵（底線版）→ binding keyword 名（帶 '-'）的對照。
(define CONFIG-KEY->NAME
  (map (lambda (row) (cons (flag-config-key (cadr row)) (cadr row))) REFLECT-FLAGS))

(define (default-config-path)
  (let ((home (getenv "HOME")))
    (and home (string-append home "/.config/llm/config.json"))))

; client＝hash-table（鍵＝binding 名帶 '-'，值＝原始字串）。回退出碼。
(define (load-config client has-config config-path)
  (let* ((named (or has-config (and (getenv CONFIG-ENV-VAR) #t)))
         (cfg-path (cond (has-config config-path)
                         ((getenv CONFIG-ENV-VAR) (getenv CONFIG-ENV-VAR))
                         (else (default-config-path)))))
    (if (not cfg-path)
        EXIT-OK
        (let ((body (read-file-string cfg-path)))
          (cond
            ((not body)
             (if named ; 明指卻讀不到＝用法錯（點名是誰指的路）
                 (begin
                   (format (current-error-port) "讀不到檔案：~A（~A 指定的 config 檔）~%"
                           cfg-path (if has-config "--config" CONFIG-ENV-VAR))
                   EXIT-USAGE)
                 EXIT-OK)) ; 探測路徑讀不到＝沒設定檔，靜默用預設
            (else (parse-config-body client body cfg-path)))))))

; 用 jq 驗 JSON 合法＋抽已知鍵。壞 JSON→用法錯；OK→寫入 client 回 EXIT-OK。
(define (parse-config-body client body path)
  (let ((tmp (string-append "/tmp/cllm_s7_cfg_" (number->string (abs (random 1000000))) ".json")))
    (call-with-output-file tmp (lambda (p) (display body p)))
    (if (not (= 0 (system (string-append "jq -e . " (shq tmp) " >/dev/null 2>&1"))))
        (begin (system (string-append "rm -f " (shq tmp)))
               (format (current-error-port) "config JSON 解析失敗（~A）~%" path)
               EXIT-USAGE)
        (let ((lines (shell-capture
                      (string-append "jq -r 'to_entries[]|\"\\(.key)\\t\\(.value)\"' " (shq tmp)))))
          (system (string-append "rm -f " (shq tmp)))
          (for-each
           (lambda (line)
             (let ((tab (char-position #\tab line)))
               (when tab
                 (let* ((k (substring line 0 tab))
                        (v (substring line (+ tab 1)))
                        (nm (assoc k CONFIG-KEY->NAME)))
                   (when nm (hash-table-set! client (cdr nm) v))))))
           (let split ((s lines) (acc '()))
             (let ((nl (char-position #\newline s)))
               (if nl (split (substring s (+ nl 1)) (cons (substring s 0 nl) acc))
                   (reverse (if (> (string-length s) 0) (cons s acc) acc))))))
          EXIT-OK))))
