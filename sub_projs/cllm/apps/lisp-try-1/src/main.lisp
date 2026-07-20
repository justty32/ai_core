;;;; src/main.lisp —— CLI 進入點（clingon）：讀 endpoint/api-key/model → 呼 (ask) → 印結果 / 分流錯誤。
;;;;
;;;; 跑法（通常經 scripts/up.sh，它會先備好 proxy＋憑證再帶環境變數進來）：
;;;;   scripts/run.sh run 用一句話介紹你自己
;;;;   scripts/run.sh run --stream 寫一首短詩
;;;;   scripts/run.sh run --check                 # 離線自檢：binding 就緒？環境變數齊？
;;;;   ./scripts/up.sh 你好                        # 一鍵佈線再跑（推薦）
;;;;
;;;; 環境變數（scripts/up.sh 會設；也可自己給）：
;;;;   APP_ENDPOINT  cllm endpoint（預設本機 proxy）
;;;;   APP_API_KEY   Bearer token（sk-ant-... 或 OAuth 換來的 key）
;;;;   APP_MODEL     模型 id（預設 claude-opus-4-8）
(in-package :lisp-try-1)

(defparameter *default-endpoint* "http://127.0.0.1:8787/v1/chat/completions")
(defparameter *default-model* "claude-opus-4-8")

(defun envd (key default)
  "環境變數 KEY，未設則回 DEFAULT。"
  (let ((v (uiop:getenv key)))
    (if (and v (plusp (length v))) v default)))

(defun do-check ()
  "離線自檢：不觸網，回報 binding 與環境狀態。回退出碼 0。"
  (format t "lisp-try-1 自檢~%")
  (format t "  cllm CL binding：~a~%"
          (if (binding-ready-p)
              "就緒（(load CLLM_LISP) → cllm:ask OK）"
              "尚未就緒 —— source ~/dev/cllm/env.sh（設 CLLM_LISP/LIBCLLM）後再試"))
  (format t "  CLLM_LISP：~a~%" (or (uiop:getenv "CLLM_LISP") "未設"))
  (format t "  LIBCLLM： ~a~%" (or (uiop:getenv "LIBCLLM") "未設（靠 LD_LIBRARY_PATH 找 libcllm.so）"))
  (format t "  APP_ENDPOINT：~a~%" (envd "APP_ENDPOINT" (format nil "~a（預設）" *default-endpoint*)))
  (format t "  APP_MODEL：   ~a~%" (envd "APP_MODEL" (format nil "~a（預設）" *default-model*)))
  (format t "  APP_API_KEY： ~a~%" (if (uiop:getenv "APP_API_KEY") "已設（不外洩內容）" "未設 → 會撞 401"))
  (format t "  失敗分流自測：~%")
  (dolist (pair '(("後端錯誤 (HTTP 401): Unauthorized" :auth)
                  ("curl: (7) Failed to connect: Connection refused" :sidecar)
                  ("something else broke" :other)))
    (multiple-value-bind (kind explain) (classify-error (first pair))
      (declare (ignore explain))
      (format t "    ~48a → ~a ~a~%" (first pair) kind
              (if (eq kind (second pair)) "OK" "MISMATCH"))))
  0)

(defun greet-handler (cmd)
  "頂層命令的動作：讀選項/環境 → 呼 ask → 印結果 / 分流失敗，並以退出碼帶語意。"
  (let ((stream (clingon:getopt cmd :stream))
        (check (clingon:getopt cmd :check))
        (endpoint-opt (clingon:getopt cmd :endpoint))
        (model-opt (clingon:getopt cmd :model))
        (args (clingon:command-arguments cmd)))
    (when check
      (uiop:quit (do-check)))
    (let ((prompt (format nil "~{~a~^ ~}" args)))
      (when (string= prompt "")
        (format *error-output* "沒給 prompt。範例：scripts/run.sh run 你好   （或 --check 自檢）~%")
        (uiop:quit 2))
      (let* ((endpoint (or endpoint-opt (envd "APP_ENDPOINT" *default-endpoint*)))
             (model (or model-opt (envd "APP_MODEL" *default-model*)))
             (api-key (uiop:getenv "APP_API_KEY"))
             (r (ask prompt
                     :endpoint endpoint :api-key api-key :model model :stream stream
                     :on-delta (when stream
                                 (lambda (piece) (write-string piece) (finish-output) nil)))))
        (if (getf r :ok)
            (progn
              (unless stream (format t "~a~%" (getf r :text)))
              (when stream (terpri))          ; 串流時補個換行收尾
              (uiop:quit 0))
            ;; 失敗：印分流訊息到 stderr，退出碼帶語意（1=一般/憑證、3=sidecar、4=no-binding）
            (progn
              (format *error-output* "✗ 失敗（~a）：~%    ~a~%" (getf r :kind) (getf r :error))
              (uiop:quit (case (getf r :kind)
                           (:sidecar 3)
                           (:no-binding 4)
                           (t 1)))))))))

(defun cli-command ()
  "定義頂層命令與選項。"
  (clingon:make-command
   :name "lisp-try-1"
   :description "用 Common Lisp 打 Anthropic API（經 cllm binding → anthropic-proxy / OpenRouter）"
   :usage "[--stream] [--check] [--endpoint URL] [--model ID] prompt..."
   :options
   (list
    (clingon:make-option :flag :description "串流輸出（逐段印）。"
                         :short-name #\s :long-name "stream" :key :stream)
    (clingon:make-option :flag :description "離線自檢（不觸網），看 binding／環境是否備妥。"
                         :long-name "check" :key :check)
    (clingon:make-option :string :description "覆寫 endpoint（預設 $APP_ENDPOINT 或本機 proxy）。"
                         :long-name "endpoint" :key :endpoint)
    (clingon:make-option :string :description "覆寫模型 id（預設 $APP_MODEL）。"
                         :short-name #\m :long-name "model" :key :model))
   :handler #'greet-handler))

(defun main ()
  "執行檔進入點（asdf:make 的 entry-point）。"
  (clingon:run (cli-command)))
