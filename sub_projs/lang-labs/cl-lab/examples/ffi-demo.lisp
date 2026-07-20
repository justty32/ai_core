;;;; C FFI 示範（cffi）：不寫 C、直接呼叫系統共享庫。
;;;; 跑法：sbcl --script examples/ffi-demo.lisp
;;;; 詳解見 docs/09-c-ffi.md
(load (merge-pathnames "quicklisp/setup.lisp" (user-homedir-pathname)))
(ql:quickload :cffi :silent t)

;; 載入共享庫
(cffi:load-foreign-library "libc.so.6")
(cffi:load-foreign-library "libm.so.6")

;; defcfun：宣告一個外部函式的簽名，之後像普通 Lisp 函式呼叫
;;   (defcfun ("C名字" lisp名字) 回傳型別 (參數 型別) ...)
(cffi:defcfun ("strlen" c-strlen) :unsigned-long
  (s :string))

(cffi:defcfun ("sqrt" c-sqrt) :double
  (x :double))

(cffi:defcfun ("cos" c-cos) :double
  (x :double))

(format t "strlen(\"hello\") = ~a~%" (c-strlen "hello"))
(format t "sqrt(2)         = ~,5f~%" (c-sqrt 2.0d0))
(format t "cos(0)          = ~,4f~%" (c-cos 0.0d0))

;; 常用型別：:void :int :unsigned-int :long :unsigned-long
;;           :float :double :string(char*) :pointer :bool
