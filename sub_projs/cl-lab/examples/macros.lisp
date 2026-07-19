;;;; Macro 示範 —— CL 的巨集是「編譯期跑的 Lisp」，Lisp 宏系統的祖宗。
;;;; 跑法：sbcl --script examples/macros.lisp
;;;; 詳解見 docs/07-macro.md
;;;; 語法：` 反引號(quasiquote)，, 取值(unquote)，,@ 展開串接(splice)

;; 1) 最簡單：my-when
(defmacro my-when (test &body body)
  `(if ,test (progn ,@body)))

(my-when t (format t "my-when 生效~%"))

;; 2) 用 gensym 避免變數捕捉（衛生）：swap 交換兩個 place
(defmacro swap (a b)
  (let ((tmp (gensym)))          ; gensym 產生獨一無二的符號，不會撞到使用者的變數
    `(let ((,tmp ,a))
       (setf ,a ,b
             ,b ,tmp))))

(let ((x 1) (y 2))
  (swap x y)
  (format t "swap -> x=~a y=~a~%" x y))

;; 3) ,@ splice：把一個串列「攤平」進模板
(defmacro run-all (&rest forms)
  `(progn ,@forms))

(run-all (format t "第一~%") (format t "第二~%"))

;; 4) 除錯 macro 的關鍵：macroexpand-1 看展開結果
(format t "展開 (my-when x (foo) (bar)):~%  ~a~%"
        (macroexpand-1 '(my-when x (foo) (bar))))
(format t "展開 (swap a b):~%  ~a~%"
        (macroexpand-1 '(swap a b)))

;; 心法：函式處理「值」，macro 處理「還沒求值的程式碼」。
;; 需要控制「求值與否 / 求值順序 / 新語法」時才用 macro，其餘用函式。
