;;;; main.lisp — SBCL --script 進入點（對齊 core-py 的 cllm/__main__.py + cli._entry）。
;;;;
;;;; 跑法：source ~/dev/cllm/env.sh 後
;;;;   sbcl --script cli/main.lisp [旗標...] [prompt...]
;;;; 先載 binding（CLLM_LISP → cllm:ask）＋ shasht（JSON，走 quicklisp）＋姊妹模組，再把
;;;; (cdr *posix-argv*) 交 cllm-cli:main。載入雜訊（quicklisp/asdf）吞進 broadcast-stream，
;;;; 保持 stdout 只剩答案。KeyboardInterrupt → 130（對齊 SIGINT 取消）。

;; 載入期例外（載 binding／模組失敗）→ 印訊息、退 1（不進互動 debugger）。
(setf sb-ext:*invoke-debugger-hook*
      (lambda (c old) (declare (ignore old))
        (format *error-output* "~&llm: 未預期錯誤：~a~%" c)
        (sb-ext:exit :code 1 :abort t)))

(let ((here (directory-namestring (or *load-truename* *load-pathname*
                                      (truename "."))))
      (binding (sb-ext:posix-getenv "CLLM_LISP")))
  (unless (and binding (probe-file binding))
    (format *error-output* "找不到 cllm binding：請設 CLLM_LISP（source ~~/dev/cllm/env.sh）~%")
    (sb-ext:exit :code 1))
  ;; 吞掉 quicklisp/asdf 的載入輸出，避免污染 CLI 的 stdout（錯誤仍走 stderr）。
  (let ((*standard-output* (make-broadcast-stream)))
    (load binding)  ; 提供 cllm:ask，並 bootstrap quicklisp
    (funcall (find-symbol "QUICKLOAD" (find-package :quicklisp-client)) :shasht)
    (dolist (f '("internal" "flags" "config" "media" "output" "argv" "reqinput" "cli"))
      (load (merge-pathnames (concatenate 'string f ".lisp") here)))))

(handler-case
    ;; *posix-argv* 首元為 runtime 名（"sbcl"）＝程式名占位，parse-argv 自 index 1 起算。
    (sb-ext:exit :code (funcall (find-symbol "MAIN" :cllm-cli)
                                sb-ext:*posix-argv*))
  (sb-sys:interactive-interrupt ()
    (format *error-output* "~&已取消~%")
    (sb-ext:exit :code 130)))
