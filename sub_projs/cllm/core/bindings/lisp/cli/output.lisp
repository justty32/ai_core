;;;; output.lisp — cllm:ask 的四個出口回呼打包成 Sink
;;;; （對齊 core-py/cllm/output.py，其對齊 core/src/cli_output.cpp 的 Sink）。
;;;;
;;;; 把「輸出接線 + 其共享狀態」收成一個 struct：文字吐 stdout、tool_calls 一行一則 JSON、
;;;; 產出媒體落檔 --media-out、錯誤吐 stderr。回呼一律回 NIL（不主動中止）。收尾狀態
;;;; （wrote-text/had-error/media-err）由 cli.lisp 讀來定退出碼。
(in-package :cllm-cli)

(defstruct sink
  media-out-dir
  (wrote-text nil)   ; 有無吐過文字（決定要不要補尾端換行）
  (had-error nil)    ; on-error 被呼叫過＝請求真失敗
  (media-err nil)    ; 媒體落檔失敗＝結果真掉了
  (media-n 0))       ; 已落檔媒體數（供編號檔名）

(defun json-quote (s)
  "把 Lisp 值序列化成單一 JSON 字面（用 binding 已在用的 shasht；CJK 不 \\u 轉義）。"
  (with-output-to-string (o) (shasht:write-json s o)))

(defun sink-on-delta-fn (sink)
  "文字回呼：逐段吐 stdout（串流與非串流皆走此路，因內核 on-text 兩者都呼叫）。"
  (lambda (piece)
    (write-string piece *standard-output*)
    (finish-output *standard-output*)
    (setf (sink-wrote-text sink) t)
    nil))

(defun sink-on-tool-fn (sink)
  "工具呼叫回呼：一行一則 JSON {\"tool\",\"id\",\"arguments\"}（arguments 原樣內嵌）。"
  (declare (ignore sink))
  (lambda (call)
    (let* ((name (getf call :name))
           (id (getf call :id))
           (args (or (getf call :arguments) "{}"))
           ;; arguments 理應是後端保證的 JSON → 原樣內嵌；壞了就當字串安全 quote。
           (inline (if (ignore-errors (shasht:read-json args)) args (json-quote args))))
      (format *standard-output* "{\"tool\":~a,\"id\":~a,\"arguments\":~a}~%"
              (json-quote name) (if id (json-quote id) "null") inline)
      (finish-output *standard-output*)
      nil)))

(defun sink-on-media-fn (sink)
  "產出媒體回呼：落檔 --media-out/llm-media-N.<ext>、路徑吐 stdout；沒給目錄＝明說丟棄。"
  (lambda (m)
    (let ((mime (getf m :mime)) (bytes (getf m :bytes)))
      (if (null (sink-media-out-dir sink))
          (format *error-output* "收到產出媒體（~a，~d bytes）但沒給 --media-out，已丟棄~%"
                  mime (length bytes))
          (progn
            (incf (sink-media-n sink))
            (let ((path (format nil "~allm-media-~d.~a"
                                (ensure-slash (sink-media-out-dir sink))
                                (sink-media-n sink) (ext-of mime))))
              (handler-case
                  (progn
                    (with-open-file (o path :direction :output :element-type '(unsigned-byte 8)
                                       :if-exists :supersede :if-does-not-exist :create)
                      (write-sequence bytes o))
                    (format *standard-output* "~a~%" path)
                    (finish-output *standard-output*))
                (error ()
                  (format *error-output* "媒體落檔失敗：~a~%" path)
                  (setf (sink-media-err sink) t))))))
      nil)))

(defun sink-on-error-fn (sink)
  "錯誤回呼：標記 had-error、吐 stderr（供退出碼定 2）。"
  (lambda (msg)
    (setf (sink-had-error sink) t)
    (format *error-output* "請求失敗：~a~%" msg)))
