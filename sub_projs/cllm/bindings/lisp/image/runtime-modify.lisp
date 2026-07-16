;;;; runtime-modify.lisp — 執行期修改程式（Lisp 活映像的精髓：跑著就改，不重啟）。
;;;; 跑：source ~/dev/env.sh 後  sbcl --script runtime-modify.lisp
(load (sb-ext:posix-getenv "CLLM_LISP"))

;;; ① 重定義傳播：早就編好的 caller，重定義 greet 後立即走新版
;;;    （呼叫綁的是「符號 greet」而非當時的函式值 → 換定義即全域生效，連舊碼也跟著換）
(defun greet (n) (format nil "v1：嗨 ~a" n))
(defun caller (n) (greet n))                 ; caller 此刻編好
(format t "① 改前：~a~%" (caller "星野"))
(defun greet (n) (format nil "v2：喲 ~a～（執行期改過）" n))  ; 重定義 greet
(format t "① 改後：~a~%" (caller "星野"))     ; caller 沒重編，卻用新 greet

;;; ② 熱換：背景執行緒跑著，中途換掉它呼叫的行為、不重啟
(defvar *tick* (lambda (i) (format nil "  v1 tick ~a" i)))
(let ((th (sb-thread:make-thread
            (lambda () (dotimes (i 6)
                         (format t "~a~%" (funcall *tick* i)) (force-output) (sleep 0.25))))))
  (sleep 0.7)
  (setf *tick* (lambda (i) (format nil "  >>> v2 熱換後 tick ~a" i))) ; 程式跑著就換
  (sb-thread:join-thread th))

;;; ③ 套到 cllm：把「怎麼問 LLM」的包裝重定義，下次呼叫立刻換行為（活映像可無限迭代 prompt/後處理）
(defun ask-wrapped (ep) (format nil "[包裝 v1] ~a" (cllm:ask "自我介紹" ep)))
(defun ask-wrapped (ep) (format nil "[包裝 v2 改過] ~a" (cllm:ask "用一句話自我介紹" ep))) ; 重定義
;; (format t "~a~%" (ask-wrapped "file://…/fake/chat/completions"))  ; 給 endpoint 才真打

(format t "~%③ 真正玩法：從 nvim(conjure/vlime) 或 emacs(SLIME/SLY) 經 SWANK/SLYNK 連進「正在跑的映像」，~%~
             即時 (defun …) 重定義任何函式、當場看效果；滿意後 (save-lisp-and-die) 把新版固化成新映像。~%")
