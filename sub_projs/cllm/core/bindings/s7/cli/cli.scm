; cli.scm — 薄 CLI 外殼：把命令列組成一次 llm-ask 的發問（對齊 core-py cli.py＝C++ cli.cpp）。
;   只做「參數解析＋I/O 接線」，真正的活（組請求／打 HTTP／解串流）全丟給 binding 的 llm-ask。
;   周邊拆到姊妹 .scm：internal（退出碼／檔案／shell）、flags（反射旗標表＋print-usage）、
;   argv（掃描）、config（三層 config）、media（三分流）、reqinput（請求輸入）、output（Sink）。
; 退出碼：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。

; 反射旗標名 → 型別。
(define (name-type name)
  (let ((row (assoc name (map (lambda (r) (cons (cadr r) (caddr r))) REFLECT-FLAGS))))
    (and row (cdr row))))

; 把原始字串依型別轉值；str 回字串，int/float 轉數（失敗回 'bad）。
(define (typed-value name raw)
  (case (name-type name)
    ((str) raw)
    ((int) (let ((n (string->number raw))) (if (and n (integer? n)) n 'bad)))
    ((float) (let ((n (string->number raw))) (if (and n (real? n)) n 'bad)))
    (else 'bad)))

; client（hash：dash-name→原始字串）→ llm-ask 的旗標關鍵字串（:endpoint v :model v …）。
; 依 REFLECT-FLAGS 順序輸出，成功回 list；型別錯回字串（壞旗標名，供 error 訊息）。
(define (client->flag-args client)
  (call-with-exit
   (lambda (return)
     (let loop ((rows REFLECT-FLAGS) (acc '()))
       (if (null? rows)
           (reverse acc)
           (let* ((name (cadr (car rows)))
                  (raw (hash-table-ref client name)))
             (if (not raw)
                 (loop (cdr rows) acc)
                 (let ((v (typed-value name raw)))
                   (if (eq? v 'bad)
                       (return (string-append "--" name))
                       (loop (cdr rows) (cons v (cons (flag-keyword name) acc))))))))))))

(define (main args)
  ; (1) 掃描 argv
  (let ((p (parse-argv args)))
    (if (integer? p)
        p
        (begin
          ; (2) prompt：位置參數 × 導管 stdin 合體
          (let* ((parts (hash-table-ref p 'prompt-parts))
                 (has-dash (member "-" parts))
                 (tty (stdin-tty?))
                 (stdin-text (cond ((not tty) (rstrip-eol (slurp-port (current-input-port))))
                                   (else ""))))
            (if (and has-dash tty)
                (begin (format (current-error-port)
                               "「-」要從 stdin 讀，但 stdin 是互動終端——用導管/檔案餵入（--help 看用法）~%")
                       EXIT-USAGE)
                (let* ((pieces (map (lambda (x) (if (string=? x "-") stdin-text x)) parts))
                       (joined (apply string-append
                                      (let interpose ((ps pieces) (first #t))
                                        (cond ((null? ps) '())
                                              (first (cons (car ps) (interpose (cdr ps) #f)))
                                              (else (cons " " (cons (car ps) (interpose (cdr ps) #f))))))))
                       (prompt (cond ((and (not has-dash) (> (string-length stdin-text) 0))
                                      (if (= 0 (string-length joined)) stdin-text
                                          (string-append joined "\n\n" stdin-text)))
                                     (else joined))))
                  (if (= 0 (string-length prompt))
                      (begin (format (current-error-port)
                                     "缺少 prompt：給位置參數或從 stdin 餵入（--help 看用法）~%")
                             EXIT-USAGE)
                      (run p prompt)))))))))

; (3) config → 旗標覆寫 →（4）組請求＋呼叫內核 → 退出碼。
(define (run p prompt)
  (let ((client (make-hash-table)))
    (let ((ec (load-config client (hash-table-ref p 'has-config) (hash-table-ref p 'config-path))))
      (if (not (= ec EXIT-OK))
          ec
          (begin
            ; 反射旗標覆寫 config（存原始字串，型別轉換在 client->flag-args）
            (for-each (lambda (rv) (hash-table-set! client (car rv) (cadr rv)))
                      (hash-table-ref p 'raw-values))
            (let ((r (build-request-inputs p)))
              (if (integer? r)
                  r
                  (let ((flag-args (client->flag-args client)))
                    (if (string? flag-args)
                        (begin (format (current-error-port) "~A 需要數值（給了非法值）~%" flag-args)
                               EXIT-USAGE)
                        (do-ask p prompt flag-args r))))))))))

; (4) 組 apply 參數、呼叫 llm-ask、flush、定退出碼。
(define (do-ask p prompt flag-args r)
  (let* ((sink (make-sink (hash-table-ref p 'media-out-dir)))
         (schema (hash-table-ref r 'schema-body))
         (tools (hash-table-ref r 'tool-defs))
         (media (hash-table-ref r 'media-items))
         (mods (hash-table-ref r 'modality-items))
         (call-args
          (append (list prompt) flag-args
                  (if (hash-table-ref p 'has-system) (list :system (hash-table-ref p 'system-text)) '())
                  (if (hash-table-ref p 'stream) (list :stream #t) '())
                  (if schema (list :schema schema) '())
                  (if (pair? tools) (list :tools tools) '())
                  (if (pair? media) (list :media media) '())
                  (if (pair? mods) (list :modalities mods) '())
                  (list :on-delta (sink-on-delta sink) :on-tool (sink-on-tool sink)
                        :on-media (sink-on-media sink) :on-error (sink-on-error sink)))))
    (catch #t
      (lambda () (apply llm-ask call-args))
      (lambda args ; 保險：內核若拋 error（理論上有 on-error 就不會），記為請求失敗
        (hash-table-set! sink 'had-error #t)
        (hash-table-set! sink 'error-msg (if (pair? (cdr args)) (format #f "~A" (cadr args)) "llm-ask error"))))
    (sink-flush! sink)
    (let ((ok (not (hash-table-ref sink 'had-error))))
      (when (and (hash-table-ref sink 'wrote-text) ok) (newline) (flush-output-port (current-output-port)))
      (cond ((not ok) EXIT-REQUEST)
            ((hash-table-ref sink 'media-err) EXIT-REQUEST)
            (else EXIT-OK)))))
