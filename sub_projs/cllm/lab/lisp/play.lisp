;;;; play.lisp — cllm CL binding：基本、串流、schema+JSON(shasht)、shell(CLI) 呼叫。
;;;; 跑：source ~/dev/cllm/env.sh 後  sbcl --script play.lisp "$CLLM_FIXTURES"
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

;; 工具呼叫：:tools 送定義，:on-tool 收模型要求的呼叫（call 是 plist，arguments 是 JSON 字串 → shasht 解）
(cllm:ask "東京天氣如何？" :endpoint (ep "fake_tool/chat/completions")
          :tools (list (list :name "get_weather" :description "查某城市天氣"
                              :parameters "{\"type\":\"object\"}"))
          :on-tool (lambda (call)
                     (let ((args (shasht:read-json (getf call :arguments))))
                       (format t "[cl] tool => ~a(city=~a, unit=~a)~%" (getf call :name)
                               (gethash "city" args) (gethash "unit" args)))
                     nil))

;; media 輸出：fake_media fixture 回文字＋audio，:on-media 收產出媒體（mime＋原始位元組）
(cllm:ask "說句話" :endpoint (ep "fake_media/chat/completions")
          :on-media (lambda (m)
                      (format t "[cl] media out => mime=~a bytes=~a~%" (getf m :mime) (length (getf m :bytes)))
                      nil))

;; media 輸入＋modalities：掛 :media（data URI）＋:modalities 送出（fixture 不看 body，這裡驗的是搬運不炸）
(format t "[cl] media in+modality => ~a~%"
        (handler-case
            (progn
              (cllm:ask "描述這張圖" :endpoint (ep "fake/chat/completions")
                        :media (list (list :url "data:image/png;base64,iVBORw0KGgo="))
                        :modalities (list (list :name "audio" :config "{\"voice\":\"alloy\",\"format\":\"wav\"}")))
              "ok")
          (error (e) (format nil "失敗：~a" e))))

;; shell 呼叫 llm CLI
(format t "[cl] shell(llm) => ~a~%"
        (string-trim '(#\Newline #\Space)
          (with-output-to-string (s)
            (sb-ext:run-program "llm" (list "你好" "--endpoint" (ep "fake/chat/completions"))
                                :search t :output s))))
