;;;; tests/main.lisp —— 離線測（不觸網、不需 binding）：驗失敗分流邏輯。
(defpackage :lisp-try-1/tests
  (:use :cl :fiveam)
  (:export :all-tests))
(in-package :lisp-try-1/tests)

(def-suite all-tests :description "lisp-try-1 離線測")
(in-suite all-tests)

(defun kind-of (msg)
  (nth-value 0 (lisp-try-1:classify-error msg)))

(test classify-auth
  (is (eq :auth (kind-of "後端錯誤 (HTTP 401): Unauthorized")) "401 → :auth")
  (is (eq :auth (kind-of "authentication_error: invalid x-api-key")) "authentication → :auth")
  (is (eq :auth (kind-of "缺 Authorization header")) "authorization → :auth"))

(test classify-sidecar
  (is (eq :sidecar
          (kind-of "curl: (7) Failed to connect to 127.0.0.1 port 8787: Connection refused"))
      "refused → :sidecar"))

(test classify-other
  (is (eq :other (kind-of "model produced garbage")) "unknown → :other")
  (is (eq :other (kind-of nil)) "nil → :other 不爆")
  (is (eq :other (kind-of "")) "空字串 → :other 不爆"))

(test binding-absent-path
  ;; binding 未就緒時，ask 要走 :no-binding、不觸網、不爆
  (unless (lisp-try-1:binding-ready-p)
    (let ((r (lisp-try-1:ask "hi" :endpoint "http://127.0.0.1:8787/v1/chat/completions")))
      (is (eq :no-binding (getf r :kind)) "binding 未就緒 → :no-binding")
      (is (null (getf r :ok)) "binding 未就緒 → :ok nil"))))
