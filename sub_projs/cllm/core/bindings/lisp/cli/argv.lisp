;;;; argv.lisp — 命令列掃描：把 argv 拆成旗標與位置參數
;;;; （對齊 core-py/cllm/argv.py，其對齊 core/src/cli.cpp 的解析段）。
;;;;
;;;; 固定旗標（--stream/--image/--schema/--system/--config/--tool/--modality/--media-out/--help/--）
;;;; 特判；反射旗標（連線／取樣，見 flags.lisp）吃下一個 argv；其餘當位置參數拼 prompt。
;;;; 「-」單獨＝stdin 插入點，其餘 '-' 開頭＝未知旗標。結果收成 PARSED-ARGS；遇 --help 或用法錯
;;;; 回 (values NIL 退出碼)，main 只讀結果、不重演解析。
(in-package :cllm-cli)

(defstruct parsed-args
  (raw-values nil)     ; list of (ask關鍵字 原始字串 型別 flag)——反射旗標
  (prompt-parts nil)
  (media-specs nil) (tool-specs nil) (modality-specs nil)
  (schema-text nil) (has-schema nil)
  (config-path nil) (has-config nil)
  (media-out-dir nil)
  (system-text nil) (has-system nil)
  (stream nil))

(defun parse-argv (args)
  "掃描 argv（ARGS 之首為程式名），回 (values PARSED-ARGS NIL)；遇 --help／用法錯回 (values NIL 碼)。"
  (let* ((v (coerce args 'vector)) (n (length v)) (i 1)
         (p (make-parsed-args)) (no-more nil))
    (labels ((need-value (flag)
               ;; 吃下一個 argv 當旗標的值；缺值回 NIL（呼叫端據此回 +exit-usage+）。
               (if (>= (+ i 1) n)
                   (progn (format *error-output* "~a 缺少值（llm --help 看用法）~%" flag) nil)
                   (progn (incf i) (aref v i))))
             (bail () (return-from parse-argv (values nil +exit-usage+))))
      (loop while (< i n) do
        (let ((a (aref v i)))
          (cond
            (no-more (push a (parsed-args-prompt-parts p)) (incf i))
            ((string= a "--") (setf no-more t) (incf i))
            ((or (string= a "--help") (string= a "-h"))
             (print-usage) (return-from parse-argv (values nil +exit-ok+)))
            ((string= a "--stream") (setf (parsed-args-stream p) t) (incf i))
            ((or (string= a "--image") (string= a "--media"))
             (let ((val (need-value a))) (unless val (bail))
               (push val (parsed-args-media-specs p)) (incf i)))
            ((string= a "--schema")
             (let ((val (need-value a))) (unless val (bail))
               (setf (parsed-args-schema-text p) val (parsed-args-has-schema p) t) (incf i)))
            ((string= a "--system")
             (let ((val (need-value a))) (unless val (bail))
               (setf (parsed-args-system-text p) val (parsed-args-has-system p) t) (incf i)))
            ((string= a "--config")
             (let ((val (need-value a))) (unless val (bail))
               (setf (parsed-args-config-path p) val (parsed-args-has-config p) t) (incf i)))
            ((string= a "--tool")
             (let ((val (need-value a))) (unless val (bail))
               (push val (parsed-args-tool-specs p)) (incf i)))
            ((string= a "--modality")
             (let ((val (need-value a))) (unless val (bail))
               (push val (parsed-args-modality-specs p)) (incf i)))
            ((string= a "--media-out")
             (let ((val (need-value a))) (unless val (bail))
               (setf (parsed-args-media-out-dir p) val) (incf i)))
            ((reflect-flag a)
             (let ((val (need-value a))) (unless val (bail))
               (destructuring-bind (flag cfgkey kw type annot) (reflect-flag a)
                 (declare (ignore flag cfgkey annot))
                 (push (list kw val type a) (parsed-args-raw-values p)))
               (incf i)))
            ((and (plusp (length a)) (char= (char a 0) #\-) (not (string= a "-")))
             ;; 「-」＝stdin 佔位符；其餘 '-' 開頭＝未知旗標
             (format *error-output* "未知旗標：~a（llm --help 看用法）~%" a)
             (bail))
            (t (push a (parsed-args-prompt-parts p)) (incf i)))))
      ;; 掃描時用 push（反序）→ 收尾一次 nreverse 復原原序。
      (setf (parsed-args-prompt-parts p) (nreverse (parsed-args-prompt-parts p))
            (parsed-args-media-specs p) (nreverse (parsed-args-media-specs p))
            (parsed-args-tool-specs p) (nreverse (parsed-args-tool-specs p))
            (parsed-args-modality-specs p) (nreverse (parsed-args-modality-specs p))
            (parsed-args-raw-values p) (nreverse (parsed-args-raw-values p)))
      (values p nil))))
