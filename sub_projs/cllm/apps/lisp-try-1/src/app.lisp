;;;; src/app.lisp —— 核心模組（純邏輯，可離線單元測試）。
;;;;
;;;; 只做兩件事：
;;;;   1. 動態解析 cllm 的 Common Lisp binding（由 CLLM_LISP 環境變數定位的一支 .lisp，
;;;;      (load) 進來後提供 cllm:ask），即使 binding 尚未就緒也不會編譯期爆掉——
;;;;      讓 --help / --check 能離線跑。
;;;;   2. 呼 cllm:ask，並把失敗照 tools/INTEGRATION.md 分流：
;;;;         401 / authentication  → 憑證問題（沒登入 / key 失效）→ 要登入或填 key
;;;;         connection refused    → sidecar 沒起（anthropic-proxy 沒跑）
;;;;      這兩類必須分清楚，別當成「模型沒回應」。
(in-package :lisp-try-1)

;;; ── binding 解析 ──────────────────────────────────────────────
;;; cllm 的 CL binding 已裝在 CLLM_LISP 指的路徑（見 ~/dev/cllm/env.sh）。
;;; 我們在執行期 (load) 它、再抓 cllm:ask，避免「binding 未就緒」時整支載不進。
;;; (load) 時 binding 內部會 quickload :cffi，並嘗試載 libcllm.so（LIBCLLM 定位）；
;;; .so 缺失只發 warning 不 error，故這裡把 warning 悶掉，只在乎 cllm:ask 是否可解析。
(defun resolve-ask ()
  "載入 cllm 的 CL binding 並回傳其 ask 函式；binding 未就緒則回 NIL。"
  (handler-case
      (progn
        (unless (find-package :cllm)
          (let ((path (uiop:getenv "CLLM_LISP")))
            (when (and path (plusp (length path)) (probe-file path))
              (handler-bind ((warning #'muffle-warning))
                (load path)))))
        (let ((pkg (find-package :cllm)))
          (when pkg
            (let ((sym (find-symbol "ASK" pkg)))
              (when (and sym (fboundp sym))
                (fdefinition sym))))))
    (error () nil)))

(defun binding-ready-p ()
  "cllm 的 CL binding 是否已可載入並取得 cllm:ask。"
  (and (resolve-ask) t))

;;; ── 失敗分流 ──────────────────────────────────────────────────
(defun classify-error (msg)
  "把 cllm:ask 的錯誤訊息分成三類，回 (values 類別 人話說明)。
類別：:auth（憑證）/ :sidecar（proxy 沒起）/ :other。"
  (let* ((raw (or msg ""))
         (low (string-downcase raw)))
    (flet ((has (needle) (search needle low))
           (has-raw (needle) (search needle raw)))
      (cond
        ((or (has "401") (has "authentication") (has "unauthorized")
             (has "authorization") (has-raw "未登入") (has-raw "token 失效"))
         (values :auth
                 (format nil "憑證問題（HTTP 401 / 未授權）：沒登入或 api_key 失效。~%~
    → Anthropic 直連（MODE=anthropic）：確認 api_key 是有效的 sk-ant-... 金鑰。~%~
    → OpenRouter OAuth（MODE=openrouter）：跑 `llm-login login` 重新帳號登入。")))
        ((or (has "connection refused") (has "couldn't connect")
             (has "could not connect") (has "connection reset")
             (has "failed to connect"))
         (values :sidecar
                 (format nil "連線被拒（connection refused）：anthropic-proxy sidecar 沒在跑。~%~
    → 先起 proxy：scripts/up.sh 會自動起（MODE=anthropic）。")))
        (t
         (values :other (format nil "其他錯誤：~a" (if (plusp (length raw)) raw "unknown"))))))))

;;; ── 對外主呼叫 ────────────────────────────────────────────────
(defun ask (prompt &key endpoint api-key model stream on-delta)
  "問一次 LLM。回傳 plist：
   成功 → (:ok t :text \"...\")
   失敗 → (:ok nil :kind :auth/:sidecar/:no-binding/:other :error \"人話說明\")
完全不觸網、不需 binding 就能被單元測試 classify-error；真呼叫才需要 binding。"
  (let ((ask-fn (resolve-ask)))
    (if (null ask-fn)
        (list :ok nil :kind :no-binding
              :error (format nil "cllm 的 CL binding 尚未就緒：(load CLLM_LISP) 找不到或失敗。~%~
    → 先 `source ~~/dev/cllm/env.sh`（設 CLLM_LISP / LIBCLLM）再跑。"))
        ;; binding 就緒：呼 cllm:ask。cllm 約定——給了 :on-error 時失敗回 NIL 並呼叫它；
        ;; 另外對任何穿出的 CL 條件（llm-error 或載 .so 失敗等）也一併捕捉成訊息。
        (let ((captured nil))
          (let ((result
                  (handler-case
                      (funcall ask-fn prompt
                               :endpoint endpoint :api-key api-key :model model
                               :stream stream :on-delta on-delta
                               :on-error (lambda (m) (setf captured m)))
                    (error (e) (setf captured (princ-to-string e)) nil))))
            (if (and result (null captured))
                (list :ok t :text result)
                (multiple-value-bind (kind explain) (classify-error captured)
                  (list :ok nil :kind kind :error explain))))))))
