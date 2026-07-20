; argv.scm — 命令列掃描：把 argv 拆成旗標與位置參數（對齊 core-py argv.py＝cli.cpp 解析段）。
;   固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/--media-out/--help/--）
;   特判；反射旗標（連線／取樣，見 flags.scm）吃下一個 argv；其餘當位置參數拼 prompt。
;   「-」單獨＝stdin 插入點，其餘 '-' 開頭＝未知旗標。
;   回 ParsedArgs（hash-table）；遇 --help／用法錯回一個整數退出碼（呼叫端據此直接返回）。

(define (make-parsed)
  (let ((p (make-hash-table)))
    (for-each (lambda (kv) (hash-table-set! p (car kv) (cdr kv)))
              (list (cons 'raw-values '()) (cons 'prompt-parts '()) (cons 'media-specs '())
                    (cons 'tool-specs '()) (cons 'modality-specs '())
                    (cons 'schema-text #f) (cons 'config-path #f) (cons 'media-out-dir #f)
                    (cons 'system-text #f) (cons 'has-schema #f) (cons 'has-config #f)
                    (cons 'has-system #f) (cons 'stream #f)))
    p))

(define (push! p key v) (hash-table-set! p key (cons v (hash-table-ref p key))))

; 反射旗標查表：flag → (name type) 或 #f。
(define (reflect-lookup flag)
  (let ((row (assoc flag (map (lambda (r) (cons (car r) (cdr r))) REFLECT-FLAGS))))
    (and row (cdr row))))

; 掃描 argv（字串 list），回 ParsedArgs 或整數退出碼。
(define (parse-argv args)
  (let ((p (make-parsed)))
    (call-with-exit
     (lambda (return)
       (let loop ((a args) (no-more-flags #f))
         (if (null? a)
             #f ; 掃完
             (let ((cur (car a)) (rest (cdr a)))
               (define (need-value flag) ; 吃下一個 argv 當值；缺值→用法錯
                 (if (null? rest)
                     (begin (format (current-error-port) "~A 缺少值（--help 看用法）~%" flag)
                            (return EXIT-USAGE))
                     (car rest)))
               (cond
                 (no-more-flags (push! p 'prompt-parts cur) (loop rest #t))
                 ((string=? cur "--") (loop rest #t))
                 ((or (string=? cur "--help") (string=? cur "-h")) (print-usage) (return EXIT-OK))
                 ((string=? cur "--stream") (hash-table-set! p 'stream #t) (loop rest no-more-flags))
                 ((or (string=? cur "--image") (string=? cur "--media"))
                  (push! p 'media-specs (need-value cur)) (loop (cdr rest) no-more-flags))
                 ((string=? cur "--schema")
                  (hash-table-set! p 'schema-text (need-value cur))
                  (hash-table-set! p 'has-schema #t) (loop (cdr rest) no-more-flags))
                 ((string=? cur "--system")
                  (hash-table-set! p 'system-text (need-value cur))
                  (hash-table-set! p 'has-system #t) (loop (cdr rest) no-more-flags))
                 ((string=? cur "--config")
                  (hash-table-set! p 'config-path (need-value cur))
                  (hash-table-set! p 'has-config #t) (loop (cdr rest) no-more-flags))
                 ((string=? cur "--tool")
                  (push! p 'tool-specs (need-value cur)) (loop (cdr rest) no-more-flags))
                 ((string=? cur "--modality")
                  (push! p 'modality-specs (need-value cur)) (loop (cdr rest) no-more-flags))
                 ((string=? cur "--media-out")
                  (hash-table-set! p 'media-out-dir (need-value cur)) (loop (cdr rest) no-more-flags))
                 ((reflect-lookup cur)
                  => (lambda (nt)
                       (push! p 'raw-values (list (car nt) (need-value cur) (cadr nt) cur))
                       (loop (cdr rest) no-more-flags)))
                 ((and (> (string-length cur) 0) (char=? (string-ref cur 0) #\-)
                       (not (string=? cur "-"))) ; 「-」＝stdin 佔位；其餘 '-' 開頭＝未知旗標
                  (format (current-error-port) "未知旗標：~A（--help 看用法）~%" cur)
                  (return EXIT-USAGE))
                 (else (push! p 'prompt-parts cur) (loop rest no-more-flags))))))
       ; 收尾：把逆序累積的 list 轉正序
       (for-each (lambda (k) (hash-table-set! p k (reverse (hash-table-ref p k))))
                 '(raw-values prompt-parts media-specs tool-specs modality-specs))
       p))))
