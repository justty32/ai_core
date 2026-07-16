;;;; example.lisp — cllm CL binding：基本、串流、schema+JSON(shasht)、shell(CLI) 呼叫。
;;;; 跑：source ~/repo/dev/env.sh 後  sbcl --script example.lisp "$CLLM_FIXTURES"
(load (or #+sbcl (sb-ext:posix-getenv "CLLM_LISP") "cllm.lisp"))
(funcall (find-symbol "QUICKLOAD" (find-package :quicklisp-client)) :shasht)

(defvar *base* (or (second sb-ext:*posix-argv*) ""))
(defun ep (n) (concatenate 'string *base* n))

(format t "[cl] ask => ~a~%" (cllm:ask "你好" (ep "fake/chat/completions")))
(format t "[cl] 串流 => ")
(cllm:ask "數到五" :endpoint (ep "fake_stream/chat/completions") :stream t
          :on-delta (lambda (p) (write-string p) (finish-output) nil))
(terpri)

;; schema → JSON → shasht 解析
(let* ((raw (cllm:ask "給我角色" :endpoint (ep "fake_json/chat/completions") :schema "{\"type\":\"object\"}"))
       (o (shasht:read-json raw)))
  (format t "[cl] json => name=~a affection=~a~%" (gethash "name" o) (gethash "affection" o)))

;; shell 呼叫 llm CLI
(format t "[cl] shell(llm) => ~a~%"
        (string-trim '(#\Newline #\Space)
          (with-output-to-string (s)
            (sb-ext:run-program "llm" (list "你好" "--endpoint" (ep "fake/chat/completions"))
                                :search t :output s))))
