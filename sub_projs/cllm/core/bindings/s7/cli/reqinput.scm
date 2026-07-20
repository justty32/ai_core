; reqinput.scm — 把請求類旗標驗證／組成 llm-ask 的請求輸入（對齊 core-py reqinput.py＝cli.cpp 組請求段）。
;   與 C++ 刻意分歧（同 core-py）：--schema/--tool/--modality 收「字面 JSON 文字」，不開檔；
;   要吃檔案內容靠 shell $(cat s.json)。解 JSON 失敗＝用法錯（退 1）。
;   --image/--media 的三分流在 media.scm。本檔把四類旗標收成 llm-ask 要的
;   schema／tools／modalities／media 輸入，回 hash-table 或整數退出碼。

(define (build-request-inputs p)
  (let ((r (make-hash-table)))
    (hash-table-set! r 'schema-body #f)
    (hash-table-set! r 'tool-defs '())
    (hash-table-set! r 'modality-items '())
    (hash-table-set! r 'media-items '())
    (call-with-exit
     (lambda (return)
       ; --schema：只驗合法；字面原樣交內核嵌入。
       (when (hash-table-ref p 'has-schema)
         (let ((txt (hash-table-ref p 'schema-text)))
           (if (json-valid? txt)
               (hash-table-set! r 'schema-body txt)
               (begin (format (current-error-port)
                              "--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）~%")
                      (return EXIT-USAGE)))))
       ; --tool：字面 tool def JSON；需 name 與非空 parameters（對齊 load_tool_def 語意）。
       (for-each
        (lambda (spec)
          (if (not (json-valid? spec))
              (begin (format (current-error-port)
                             "--tool 不是合法 JSON（收字面文字非路徑；長內容用 $(cat t.json)）~%")
                     (return EXIT-USAGE))
              (let ((name (jq-on spec "-r '.name // empty'"))
                    (params (jq-on spec "-c '.parameters // empty'"))
                    (desc (jq-on spec "-r '.description // \"\"'")))
                (if (or (= 0 (string-length name)) (= 0 (string-length params)))
                    (begin (format (current-error-port) "--tool 缺 name 或 parameters~%")
                           (return EXIT-USAGE))
                    (hash-table-set! r 'tool-defs
                      (cons (list name desc params) (hash-table-ref r 'tool-defs)))))))
        (hash-table-ref p 'tool-specs))
       ; --modality：「名」或「名=<字面JSON>」。
       (for-each
        (lambda (spec)
          (let* ((eq (char-position #\= spec))
                 (name (if eq (substring spec 0 eq) spec))
                 (cfg (and eq (substring spec (+ eq 1)))))
            (cond
              ((= 0 (string-length name))
               (format (current-error-port)
                       "--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）~%")
               (return EXIT-USAGE))
              ((and cfg (not (json-valid? cfg)))
               (format (current-error-port)
                       "--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）~%")
               (return EXIT-USAGE))
              (else (hash-table-set! r 'modality-items
                      (cons (cons name cfg) (hash-table-ref r 'modality-items)))))))
        (hash-table-ref p 'modality-specs))
       ; --image/--media：三分流取值（media.scm）。
       (for-each
        (lambda (spec)
          (let ((item (build-media-item spec)))
            (if item
                (hash-table-set! r 'media-items (cons item (hash-table-ref r 'media-items)))
                (return EXIT-USAGE))))
        (hash-table-ref p 'media-specs))
       ; --media-out 目錄須存在。
       (let ((dir (hash-table-ref p 'media-out-dir)))
         (when (and dir (not (= 0 (system (string-append "test -d " (shq dir))))))
           (format (current-error-port) "--media-out 不是可用目錄：~A~%" dir)
           (return EXIT-USAGE)))
       ; 正序化後回傳。
       (for-each (lambda (k) (hash-table-set! r k (reverse (hash-table-ref r k))))
                 '(tool-defs modality-items media-items))
       r))))
