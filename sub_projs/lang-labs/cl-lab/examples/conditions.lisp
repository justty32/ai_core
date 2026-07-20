;;;; 條件系統（conditions / restarts）—— CL 最強大、也最與眾不同的一塊。
;;;; 跑法：sbcl --script examples/conditions.lisp
;;;; 詳解見 docs/08-conditions.md
;;;; 重點：錯誤「發生的地方」與「怎麼處理」可以完全分離，而且處理時
;;;;       堆疊還沒被拆掉——可以選一個 restart 讓程式「從錯誤點繼續」。

;; 1) 自訂條件（像自訂 exception，但更通用）
(define-condition my-error (error)
  ((msg :initarg :msg :reader msg)))

;; 2) handler-case：最像其它語言的 try/catch
(format t "handler-case: ~a~%"
  (handler-case (error 'my-error :msg "boom")
    (my-error (e) (format nil "抓到：~a" (msg e)))))

;; 3) restart-case + handler-bind：分離「發生」與「處理」
;;    safe-div 在除以零時 signal 一個 error，並「提供」一個名為 use-zero 的 restart。
;;    呼叫端可以用 handler-bind 攔截、決定去 invoke 哪個 restart。
(defun safe-div (a b)
  (restart-case (if (zerop b) (error "除以零") (/ a b))
    (use-zero () 0)))

(format t "1/0（選 use-zero restart）-> ~a~%"
  (handler-bind ((error (lambda (c) (declare (ignore c))
                          (invoke-restart 'use-zero))))
    (safe-div 1 0)))
(format t "6/2（正常）             -> ~a~%" (safe-div 6 2))

;; 4) unwind-protect：像 finally，保證清理一定跑
(unwind-protect
     (format t "body 執行~%")
  (format t "cleanup 一定執行~%"))

;; 5) ignore-errors：最省事的「出錯就回 nil + 條件」（回傳兩個值）
(format t "ignore-errors: ~a~%" (multiple-value-list (ignore-errors (error "故意出錯"))))
