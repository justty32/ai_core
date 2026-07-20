; media.scm — --image/--media 的 MIME 對照與三分流取值（對齊 core-py media.py）。
;   mime-of／ext-of 對齊 cli_config.cpp 的同名對照表。build-media-item 是三分流取值。
;   ⚠ s7 無 base64：.json 描述子的 {mime,data} 形，改「重組 data URI」交內核（免自己 decode），
;     等價於直通一個 data:<mime>;base64,<data> 的 url——優雅降級，見 README 說明。

(define (mime-of path)
  (let* ((dot (let loop ((i (- (string-length path) 1)))
                (cond ((< i 0) #f) ((char=? (string-ref path i) #\.) i) (else (loop (- i 1))))))
         (ext (if dot (string-downcase (substring path (+ dot 1))) "")))
    (cond ((or (string=? ext "png")) "image/png")
          ((or (string=? ext "jpg") (string=? ext "jpeg")) "image/jpeg")
          ((string=? ext "gif") "image/gif")
          ((string=? ext "webp") "image/webp")
          ((string=? ext "wav") "audio/wav")
          ((string=? ext "mp3") "audio/mpeg")
          (else "application/octet-stream"))))

(define (ext-of mime)
  (cond ((string=? mime "image/png") "png")
        ((string=? mime "image/jpeg") "jpg")
        ((string=? mime "image/gif") "gif")
        ((string=? mime "image/webp") "webp")
        ((string=? mime "audio/wav") "wav")
        ((string=? mime "audio/mpeg") "mp3")
        (else "bin")))

(define (starts-with? s prefix)
  (let ((n (string-length prefix)))
    (and (>= (string-length s) n) (string=? (substring s 0 n) prefix))))
(define (ends-with? s suffix)
  (let ((n (string-length suffix)) (m (string-length s)))
    (and (>= m n) (string=? (substring s (- m n)) suffix))))

; 一個 --image/--media 值 → binding media item (url mime data)；失敗印 stderr 回 #f。
(define (build-media-item spec)
  (let ((low (string-downcase spec)))
    (cond
      ; 1. data:／http(s):// URL → 直通當 url（帶 url 就不帶 bytes）。
      ((or (starts-with? spec "data:") (starts-with? low "http://") (starts-with? low "https://"))
       (list spec #f #f))
      ; 2. .json 描述子（用副檔名判定）→ jq 讀，直通、不重算。
      ((ends-with? low ".json") (media-from-descriptor spec))
      ; 3. 其餘＝二進位圖檔：讀檔 raw bytes ＋ mime（副檔名推），交內核 base64。
      (else
       (let ((raw (read-file-string spec)))
         (if raw (list #f (mime-of spec) raw)
             (begin (format (current-error-port) "讀不到檔案：~A（--image/--media）~%" spec) #f)))))))

; .json 描述子：接受 {"url":…} 或 {"mime":…,"data":…}（後者重組 data URI）。
(define (media-from-descriptor spec)
  (let ((body (read-file-string spec)))
    (if (not body)
        (begin (format (current-error-port) "讀不到檔案：~A（--image/--media 描述子）~%" spec) #f)
        (let ((tmp (string-append "/tmp/cllm_s7_md_" (number->string (abs (random 1000000))) ".json")))
          (call-with-output-file tmp (lambda (p) (display body p)))
          (if (not (= 0 (system (string-append "jq -e . " (shq tmp) " >/dev/null 2>&1"))))
              (begin (system (string-append "rm -f " (shq tmp)))
                     (format (current-error-port) "media 描述子 JSON 解析失敗（~A）~%" spec) #f)
              (let ((url  (shell-capture (string-append "jq -r '.url  // empty' " (shq tmp))))
                    (mime (shell-capture (string-append "jq -r '.mime // empty' " (shq tmp))))
                    (data (shell-capture (string-append "jq -r '.data // empty' " (shq tmp)))))
                (system (string-append "rm -f " (shq tmp)))
                (cond
                  ((> (string-length url) 0) (list url #f #f)) ; 直通 url
                  ((and (> (string-length mime) 0) (> (string-length data) 0))
                   (list (string-append "data:" mime ";base64," data) #f #f)) ; 重組 data URI
                  (else
                   (format (current-error-port)
                           "media 描述子形狀不符——需 {\"url\":…} 或 {\"mime\":…,\"data\":…}（~A）~%" spec)
                   #f))))))))
