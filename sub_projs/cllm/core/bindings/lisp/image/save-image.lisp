;;;; save-image.lisp — 把 cllm 綁定＋你的碼「烤」進 SBCL 映像（save-lisp-and-die）。
;;;;
;;;; 產可執行映像：  sbcl --script save-image.lisp exe    → ./cllm-image
;;;; 產 core 映像：   sbcl --script save-image.lisp core   → cllm.core
;;;; 需先  source ~/dev/cllm/env.sh（提供 CLLM_LISP 綁定路徑、LIBCLLM 定位 .so）。
;;;;
;;;; 重點：CFFI 用 define-foreign-library/use-foreign-library 註冊 libcllm，SBCL 重啟映像時會
;;;; 自動重載該 .so——所以烤進映像後，cllm:ask 開箱即用、不必再 quickload/load（實測冷載 ~268ms
;;;; → 映像啟動 ~5ms）。

(load (sb-ext:posix-getenv "CLLM_LISP")) ; 載綁定：quickload CFFI + load-foreign-library libcllm

(defun main ()
  "可執行映像的進入點：cllm-image <prompt> [endpoint]"
  (let* ((a sb-ext:*posix-argv*) (prompt (or (second a) "你好")) (ep (third a)))
    (handler-case
        (format t "~a~%" (if ep (cllm:ask prompt ep) (cllm:ask prompt)))
      (cllm:llm-error (e) (format *error-output* "~a~%" e) (sb-ext:exit :code 2))))
  (sb-ext:exit :code 0))

(let ((mode (or (second sb-ext:*posix-argv*) "exe")))
  (if (string= mode "core")
      ;; core：當「快啟動的基底」給別的腳本用 → sbcl --core cllm.core --eval '(cllm:ask …)'
      (sb-ext:save-lisp-and-die "cllm.core")
      ;; exe：獨立可執行檔 → ./cllm-image 你好 <endpoint>
      (sb-ext:save-lisp-and-die "cllm-image"
                                :executable t :toplevel #'main :save-runtime-options t)))
