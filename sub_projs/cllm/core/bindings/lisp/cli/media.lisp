;;;; media.lisp — --image/--media 的 MIME 對照與三分流取值（對齊 core-py/cllm/media.py）。
;;;;
;;;; mime-of／ext-of 對齊 cli_config.cpp 的同名對照表。build-media-item 是 core-py 特有的取值
;;;; 分流：讓 --media 除了讀二進位圖檔，也能吃「已編碼」形式（data:/http URL、或 .json 描述子），
;;;; 省掉每次重算 base64。b64decode 為純標準庫實作，不引額外相依。
(in-package :cllm-cli)

(defparameter *mime-table*
  '(("png" . "image/png") ("jpg" . "image/jpeg") ("jpeg" . "image/jpeg")
    ("gif" . "image/gif") ("webp" . "image/webp") ("wav" . "audio/wav")
    ("mp3" . "audio/mpeg")))
(defparameter *ext-table*
  '(("image/png" . "png") ("image/jpeg" . "jpg") ("image/gif" . "gif")
    ("image/webp" . "webp") ("audio/wav" . "wav") ("audio/mpeg" . "mp3")))

(defun mime-of (path)
  "副檔名 → MIME（對齊 cli_config.cpp 的 mime_of；未知回 application/octet-stream）。"
  (let* ((dot (position #\. path :from-end t))
         (ext (when dot (string-downcase (subseq path (1+ dot))))))
    (or (cdr (assoc ext *mime-table* :test #'equal)) "application/octet-stream")))

(defun ext-of (mime)
  "MIME → 落檔副檔名（對齊 cli_config.cpp 的 ext_of；未知回 bin）。"
  (or (cdr (assoc mime *ext-table* :test #'equal)) "bin"))

(defun b64decode (string)
  "標準 base64 解碼 → octet vector（純標準庫；非法字元 signal ERROR）。"
  (let ((tbl (load-time-value
              (let ((a (make-array 128 :initial-element -1))
                    (al "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"))
                (dotimes (i (length al)) (setf (aref a (char-code (char al i))) i))
                a)))
        (out (make-array 0 :element-type '(unsigned-byte 8) :adjustable t :fill-pointer 0))
        (acc 0) (nbits 0))
    (loop for ch across string
          for code = (char-code ch)
          unless (member ch '(#\= #\Newline #\Return #\Space #\Tab)) do
            (let ((v (and (< code 128) (aref tbl code))))
              (when (or (null v) (minusp v)) (error "非法 base64 字元"))
              (setf acc (logior (ash acc 6) v) nbits (+ nbits 6))
              (when (>= nbits 8)
                (decf nbits 8)
                (vector-push-extend (ldb (byte 8 nbits) acc) out))))
    (coerce out '(simple-array (unsigned-byte 8) (*)))))

(defun build-media-item (spec)
  "--image/--media 一個值 → cllm:ask 的 media plist（三分流）。成功回 plist、失敗印 stderr 回 NIL。
1) data:／http(s):// URL → 直接當 :url 送、不編碼。
2) .json 副檔名 → 讀檔當「預先算好的 media 描述子」直通：{\"url\":…} 或 {\"mime\":…,\"data\":…}。
3) 其餘（二進位圖檔路徑）→ 讀檔 + 由內核 base64 編碼。"
  (let ((low (string-downcase spec)))
    (cond
      ((or (prefixp "data:" low) (prefixp "http://" low) (prefixp "https://" low))
       (list :url spec))  ; URL 直通
      ((suffixp ".json" low)  ; 預先算好的 media 描述子
       (let (body desc)
         (handler-case (setf body (read-file-text spec))
           (error ()
             (format *error-output* "讀不到檔案：~a（--image/--media 描述子）~%" spec)
             (return-from build-media-item nil)))
         (handler-case (setf desc (shasht:read-json body))
           (error ()
             (format *error-output* "media 描述子 JSON 解析失敗（~a）~%" spec)
             (return-from build-media-item nil)))
         (cond
           ((and (hash-table-p desc) (gethash "url" desc))
            (list :url (gethash "url" desc)))  ; 直通 url、不編碼
           ((and (hash-table-p desc) (gethash "mime" desc)
                 (nth-value 1 (gethash "data" desc)))
            (let (raw)
              (handler-case (setf raw (b64decode (gethash "data" desc)))
                (error ()
                  (format *error-output* "media 描述子的 data 不是合法 base64（~a）~%" spec)
                  (return-from build-media-item nil)))
              (list :url "" :mime (gethash "mime" desc) :bytes raw)))  ; decode→bytes 走既有路
           (t (format *error-output*
                      "media 描述子形狀不符——需 {\"url\":…} 或 {\"mime\":…,\"data\":…}（~a）~%" spec)
              nil))))
      (t  ; 二進位圖檔：讀檔 + 內核 base64
       (let (raw)
         (handler-case (setf raw (read-file-bytes spec))
           (error ()
             (format *error-output* "讀不到檔案：~a（--image/--media）~%" spec)
             (return-from build-media-item nil)))
         (list :url "" :mime (mime-of spec) :bytes raw))))))
