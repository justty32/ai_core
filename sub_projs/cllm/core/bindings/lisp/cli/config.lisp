;;;; config.lisp — 三層 config 來源解析
;;;; （對齊 core-py/cllm/config.py，其對齊 core/src/cli_config.cpp 的 load_into）。
;;;;
;;;; 來源優先序（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。本模組只處理「config 檔」
;;;; 這層；命令列旗標覆寫在 cli.lisp。config 檔路徑：--config ＞ 環境變數 ＞ ~/.config/llm/config.json。
;;;; 未列鍵靜默忽略（lenient；與 C++ glaze 的嚴格解析刻意分歧）。
(in-package :cllm-cli)

(defun default-config-path ()
  "~/.config/llm/config.json（對齊 config.py：靠 HOME；無 HOME 回 NIL）。"
  (let ((home (sb-ext:posix-getenv "HOME")))
    (when (and home (plusp (length home)))
      (format nil "~a/.config/llm/config.json" home))))

(defun load-config (client has-config config-path)
  "三層 config 前二層（對齊 config.py 的 load_config）。CLIENT 是 hash-table（ask關鍵字→值）。
回退出碼：明指卻讀不到／JSON 壞＝+exit-usage+；其餘＝+exit-ok+。"
  (let (cfg (named nil))
    (cond (has-config (setf cfg config-path named t))
          ((let ((e (sb-ext:posix-getenv +config-env-var+))) (and e (plusp (length e))))
           (setf cfg (sb-ext:posix-getenv +config-env-var+) named t))
          (t (setf cfg (default-config-path))))
    (unless cfg (return-from load-config +exit-ok+))
    (let (body)
      (handler-case (setf body (read-file-text cfg))
        (error ()
          (if named  ; 明指卻讀不到＝用法錯（點名是誰指的路）
              (progn
                (format *error-output* "讀不到檔案：~a（~a 指定的 config 檔）~%"
                        cfg (if has-config "--config" +config-env-var+))
                (return-from load-config +exit-usage+))
              (return-from load-config +exit-ok+))))  ; 探測路徑讀不到＝沒設定檔，靜默用預設
      (let (data)
        (handler-case (setf data (shasht:read-json body))
          (error ()
            (format *error-output* "config JSON 解析失敗（~a）~%" cfg)
            (return-from load-config +exit-usage+)))
        (when (hash-table-p data)
          (dolist (row *reflect-flags*)
            (destructuring-bind (flag cfgkey kw type annot) row
              (declare (ignore flag type annot))
              (multiple-value-bind (v present) (gethash cfgkey data)
                (when present (setf (gethash kw client) v))))))  ; 未列鍵不會進來＝lenient
        +exit-ok+))))
