;;;; hello.lisp — comfy 糖示範
;;;;
;;;; 跑法（cwd 需在 comfy/）： sbcl --script examples/hello.lisp

(require :asdf)
(let ((root (uiop:pathname-parent-directory-pathname
             (uiop:pathname-directory-pathname *load-truename*))))
  (asdf:load-asd (merge-pathnames "comfy.asd" root))
  (asdf:load-system "comfy"))

(use-package :comfy)   ; 把 true / false / enable-comfy-syntax 帶進 CL-USER

;; 啟用 'a' 字元語法（在檔頭 eval-when，之後每一段才會用 comfy readtable 讀）。
(eval-when (:compile-toplevel :load-toplevel :execute)
  (enable-comfy-syntax))

(format t "~&true = ~s，false = ~s~%" true false)
(format t "第一個字母 'a' 讀成 ~s（型別 ~a）~%" 'a' (type-of 'a'))
(format t "換行 '\\n' 讀成 ~s，Tab '\\t' 讀成 ~s~%" '\n' '\t')
(format t "quote 仍然正常： '(1 2 3) → ~s~%" '(1 2 3))
(format t "布林糖上場： (if true 'y' 'n') → ~s~%" (if true 'y' 'n'))
