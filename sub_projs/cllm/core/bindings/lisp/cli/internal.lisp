;;;; internal.lisp — CLI 共用的退出碼、環境變數鍵、檔案讀取與字串小工具
;;;; （對齊 core-py/cllm/internal.py，其對齊 core/src/cli_internal.hpp）。
;;;;
;;;; 葉模組：只依標準庫、不用套件內其他 CLI 模組，作為 flags/config/media/output/argv/
;;;; reqinput/cli 的共用底，把依賴收成一張無環 DAG。此檔也定義整組 CLI 的 package。
(defpackage :cllm-cli
  (:use :cl)
  (:export :main))
(in-package :cllm-cli)

;; 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。
(defparameter +exit-ok+ 0)
(defparameter +exit-usage+ 1)
(defparameter +exit-request+ 2)
(defparameter +exit-cancel+ 130)

;; env 只用來指定 config 檔路徑，不存任何設定值（對齊 cli_internal.hpp 的 kConfigEnvVar）。
(defparameter +config-env-var+ "LLM_CLI_CONFIG")

(defun read-file-bytes (path)
  "整檔讀成 octet vector（媒體圖檔等）。讀不到 signal FILE-ERROR，由呼叫端轉退出碼。"
  (with-open-file (s path :element-type '(unsigned-byte 8))
    (let ((buf (make-array (file-length s) :element-type '(unsigned-byte 8))))
      (read-sequence buf s)
      buf)))

(defun read-file-text (path)
  "整檔讀成 UTF-8 文字（config／media 描述子等）。讀不到 signal FILE-ERROR。"
  (sb-ext:octets-to-string (read-file-bytes path) :external-format :utf-8))

(defun prefixp (prefix s)
  "S 是否以 PREFIX 起頭（大小寫敏感；呼叫端自行先 downcase）。"
  (and (>= (length s) (length prefix))
       (string= s prefix :end1 (length prefix))))

(defun suffixp (suffix s)
  "S 是否以 SUFFIX 結尾。"
  (and (>= (length s) (length suffix))
       (string= s suffix :start1 (- (length s) (length suffix)))))

(defun ensure-slash (dir)
  "確保目錄字串以 / 結尾（供組落檔路徑）。"
  (if (and (plusp (length dir)) (char= (char dir (1- (length dir))) #\/))
      dir
      (concatenate 'string dir "/")))

(defun dir-usable-p (dir)
  "DIR 是否為可用目錄（存在且是目錄）；否則回 NIL（對齊 os.path.isdir 用法）。"
  (ignore-errors (truename (ensure-slash dir))))
