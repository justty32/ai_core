;;;; package.lisp — comfy 舒適層的套件定義
;;;;
;;;; comfy = 一層薄薄的「順手糖」，蓋在標準 Common Lisp 上：
;;;;   - true / false 常數（不必再寫 t / nil）
;;;;   - 'a' 字元字面量讀取器（不必再寫 #\a）
;;;; 只用標準 CL，零外部相依。細節見 comfy.lisp 與 README。

(defpackage #:comfy
  (:use #:cl)
  (:documentation "舒適 Common Lisp 地基：true/false 常數 + 'a' 字元讀取器。")
  (:export
   ;; 布林糖
   #:true
   #:false
   ;; 讀取器 / readtable
   #:*comfy-readtable*
   #:comfy-quote-reader
   #:enable-comfy-syntax
   #:with-comfy-syntax))
