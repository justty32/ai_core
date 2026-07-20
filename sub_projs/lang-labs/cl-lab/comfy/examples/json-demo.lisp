;;;; json-demo.lisp — comfy 糖 + jzon（Quicklisp）的 JSON round-trip 示範
;;;;
;;;; 跑法（cwd 需在 comfy/，且已裝 Quicklisp）： sbcl --script examples/json-demo.lisp
;;;; 註：--script 不載入 ~/.sbclrc，故這裡手動 load quicklisp setup；
;;;;     在 Alive/REPL 裡則靠 ~/.sbclrc 自動載入，直接 (ql:quickload :com.inuoe.jzon) 即可。

(require :asdf)
(load (merge-pathnames "quicklisp/setup.lisp" (user-homedir-pathname)))
(ql:quickload :com.inuoe.jzon :silent t)

(let ((root (uiop:pathname-parent-directory-pathname
             (uiop:pathname-directory-pathname *load-truename*))))
  (asdf:load-asd (merge-pathnames "comfy.asd" root))
  (asdf:load-system "comfy"))

(use-package :comfy)
(eval-when (:compile-toplevel :load-toplevel :execute)
  (enable-comfy-syntax))

;; comfy 的 C 風格字串讓 JSON 字面量裡的 \" 很自然（跟 C 一樣）。
(let* ((in  "{\"msg\":\"你好貓娘\",\"n\":42,\"ok\":true,\"arr\":[1,2,3]}")
       (obj (com.inuoe.jzon:parse in)))
  (format t "~&解析： msg=~a n=~a ok=~a arr0=~a~%"
          (gethash "msg" obj) (gethash "n" obj) (gethash "ok" obj)
          (aref (gethash "arr" obj) 0))
  (format t "重序列化： ~a~%" (com.inuoe.jzon:stringify obj))
  ;; 也可以反向：CL 資料 → JSON
  (let ((ht (make-hash-table :test 'equal)))
    (setf (gethash "greet" ht) "喵"
          (gethash "lucky" ht) 7
          (gethash "on" ht) true)          ; ← comfy 的 true
    (format t "自組 → JSON： ~a~%" (com.inuoe.jzon:stringify ht))))
