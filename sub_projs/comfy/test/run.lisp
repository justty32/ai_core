;;;; run.lisp — comfy 的無框架 sanity 測試
;;;;
;;;; 跑法（cwd 需在 comfy/）：
;;;;   sbcl --script test/run.lisp
;;;; 全綠印 "ALL GREEN" 並 exit 0；任何 assert 掛掉 exit 非 0。

(require :asdf)

;; 由本腳本位置回推 comfy/ 根，載入系統（不依賴 cwd 註冊）。
(let ((root (uiop:pathname-parent-directory-pathname
             (uiop:pathname-directory-pathname *load-truename*))))
  (asdf:load-asd (merge-pathnames "comfy.asd" root))
  (asdf:load-system "comfy"))

(defun run ()
  ;; (1) 布林糖
  (assert (eq comfy:true  t))
  (assert (eq comfy:false nil))

  ;; (2) 'a' 字元讀取器：在 comfy readtable 下讀字串來驗
  (let ((*readtable* comfy:*comfy-readtable*))
    ;; 字元字面量
    (assert (eql (read-from-string "'a'")   #\a))
    (assert (eql (read-from-string "'Z'")   #\Z))
    (assert (eql (read-from-string "'7'")   #\7))
    ;; 轉義字元字面量
    (assert (eql (read-from-string "'\\n'") #\Newline))
    (assert (eql (read-from-string "'\\t'") #\Tab))
    (assert (eql (read-from-string "'\\s'") #\Space))
    (assert (eql (read-from-string "'\\\\'") #\\))     ; '\\' → 反斜線
    (assert (eql (read-from-string "'\\''") #\'))      ; '\'' → 單引號（轉義版）
    (assert (eql (read-from-string "'''")   #\'))      ; '''  → 單引號（緊跟版）
    ;; quote 一切照舊
    (assert (equal (read-from-string "'x")       '(quote x)))
    (assert (equal (read-from-string "'foo-bar") '(quote foo-bar)))
    (assert (equal (read-from-string "'(1 2 3)") '(quote (1 2 3))))
    (assert (equal (read-from-string "''a")      '(quote (quote a)))))

  (format t "~&ALL GREEN — 布林糖 + 'a' 字元讀取器全數通過。~%")
  t)

(handler-case (progn (run) (uiop:quit 0))
  (error (e)
    (format *error-output* "~&FAIL: ~a~%" e)
    (uiop:quit 1)))
