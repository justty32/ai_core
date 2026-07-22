;;;; cllm.lisp — cllm 的 Common Lisp binding：用 CFFI 呼叫 libcllm 的 C ABI（唯一入口 llm_ask）。
;;;;
;;;; API 對齊其他語言（ask + on-delta 串流回呼；CL 用關鍵字參數）：
;;;;   (cllm:ask "你好")                                   ; 只給 prompt（走內建 localhost）
;;;;   (cllm:ask "你好" "http://…/chat/completions")       ; prompt + endpoint（位置形式）
;;;;   (cllm:ask "你好" :model "local-model" :temperature 0.7)
;;;;   (cllm:ask "你好" :system "你是一隻傲嬌的貓")             ; system role 指示（在 user 前插一則）
;;;;   (cllm:ask "數到五" :stream t                         ; 串流：逐段進 on-delta
;;;;             :on-delta (lambda (piece) (write-string piece) (finish-output) nil))  ; 回真值可中止
;;;;   (cllm:ask "東京天氣如何？"                           ; 工具：:tools 送定義，:on-tool 收呼叫
;;;;             :tools (list (list :name "get_weather" :description "查天氣"
;;;;                                 :parameters "{\"type\":\"object\"}"))
;;;;             :on-tool (lambda (call) (print (getf call :name)) nil))               ; 回真值可中止
;;;;   (cllm:ask "描述這張圖"                               ; 輸入媒體＋想要的輸出模態
;;;;             :media (list (list :url "data:image/png;base64,…"))
;;;;             :modalities (list (list :name "audio" :config "{\"voice\":\"alloy\"}"))
;;;;             :on-media (lambda (m) (print (getf m :mime)) nil))                    ; 回真值可中止
;;;; 回「完整組合後的答案字串」；失敗時：給了 :on-error 就呼叫它並回 NIL，否則 signal LLM-ERROR。
;;;;
;;;; 載入：先確保 quicklisp 可用（~/.sbclrc 已載），(load "cllm.lisp") 即自動 quickload CFFI。
;;;; libcllm.so 定位：優先環境變數 LIBCLLM，否則靠系統/LD_LIBRARY_PATH 找 libcllm.so。
;;;; 範圍：prompt＋連線/取樣選項＋stream＋schema＋tools＋media＋modalities，四個出口回呼全收
;;;; （on-delta／on-tool／on-media／on-error）。

;; 確保 CFFI 可用：優先既載，其次 quicklisp（--script 不讀 ~/.sbclrc，故必要時自載 setup.lisp）。
;; 不寫字面 ql: —— 沒載 quicklisp 時連讀取都會 package error。
(eval-when (:compile-toplevel :load-toplevel :execute)
  (unless (find-package :cffi)
    (unless (find-package :quicklisp-client)
      (let ((setup (merge-pathnames "quicklisp/setup.lisp" (user-homedir-pathname))))
        (when (probe-file setup) (load setup))))
    (let ((ql (find-package :quicklisp-client)))
      (if ql
          (funcall (find-symbol "QUICKLOAD" ql) :cffi)
          (error "cllm.lisp 需要 CFFI：請先 (ql:quickload :cffi)，或裝 quicklisp 到 ~~/quicklisp。")))))

(defpackage :cllm
  (:use :cl :cffi)
  (:export :ask :llm-error :llm-error-message))
(in-package :cllm)

;; ── 載入 libcllm.so（LIBCLLM 環境變數優先；否則走預設搜尋路徑）──
(define-foreign-library libcllm
  (t (:default "libcllm")))

(let ((explicit #+sbcl (sb-ext:posix-getenv "LIBCLLM") #-sbcl nil))
  (handler-case
      (if (and explicit (plusp (length explicit)))
          (load-foreign-library explicit)
          (load-foreign-library 'libcllm))
    (error (e)
      (warn "cllm: 載入 libcllm.so 失敗（設 LIBCLLM 或 LD_LIBRARY_PATH）：~a" e))))

;; ── C ABI 結構鏡像（欄位順序/型別須與 cabi_*.h 一致；字串欄用 :pointer 自行管理生命週期）──
(defcstruct client
  (endpoint :pointer) (api-key :pointer) (model :pointer)
  (timeout-ms :long) (field-mask :uint)
  (temperature :float) (top-p :float) (presence-penalty :float) (frequency-penalty :float)
  (max-tokens :int) (seed :int))
(defcstruct schema (name :pointer) (json :pointer))
;; ── 「送出」那半邊（cabi_request.h）：tool 定義／輸入媒體／想要的輸出模態 ──
(defcstruct tool-def (name :pointer) (description :pointer) (parameters :pointer))
(defcstruct media-in (url :pointer) (mime :pointer) (data :pointer) (len :size))
(defcstruct modality (name :pointer) (config :pointer))
(defcstruct request
  ;; ⚠ CFFI 依宣告順序算 offset——`system` 是 prompt 之後第二欄（對齊 cabi_request.h 的
  ;;   llm_request_t）。漏掉它會讓其後所有欄位位移一個指標寬，stream 落錯槽、media/modalities
  ;;   讀錯位址。見 gotchas/windows.md 與 python binding 的同款註記。
  (prompt :pointer) (system :pointer) (schema :pointer)
  (tools :pointer) (tools-count :size)
  (media :pointer) (media-count :size)
  (modalities :pointer) (modalities-count :size)
  (stream :int))
;; ── 「收回」那半邊（cabi_response.h）：模型要求的工具呼叫／模型產出的媒體 ──
(defcstruct tool-call (id :pointer) (name :pointer) (arguments :pointer))
(defcstruct media-out (mime :pointer) (data :pointer) (len :size))
(defcstruct handlers
  (on-text :pointer) (text-user :pointer)
  (on-tool :pointer) (tool-user :pointer)
  (on-media :pointer) (media-user :pointer)
  (on-error :pointer) (error-user :pointer)
  (on-usage :pointer) (usage-user :pointer)) ; token 用量（本綁定未外露，恆 NULL）

;; field_mask 位旗標（cabi_client.h 的 LLM_FIELD_*）
(defconstant +f-temperature+ 1)
(defconstant +f-top-p+ 2)
(defconstant +f-presence+ 4)
(defconstant +f-frequency+ 8)
(defconstant +f-max-tokens+ 16)
(defconstant +f-seed+ 32)

(defcfun ("llm_ask" %llm-ask) :int
  (ctx :pointer) (client :pointer) (req :pointer) (handlers :pointer))

(define-condition llm-error (error)
  ((message :initarg :message :reader llm-error-message))
  (:report (lambda (c s) (format s "cllm: ~a" (llm-error-message c)))))

;; llm_ask 阻塞、handler 在同一 thread 上被呼叫 → 用動態變數把每次呼叫的狀態餵給全域 callback。
(defvar *acc*) (defvar *on-delta*) (defvar *on-error*) (defvar *cb-error*) (defvar *err-msg*)
(defvar *on-tool*) (defvar *on-media*)

;; 空指標當「未給」——CFFI 沒有天然 NULL 字串語意，一律轉 NIL。
(defun %opt-str (ptr) (unless (null-pointer-p ptr) (foreign-string-to-lisp ptr :encoding :utf-8)))

(defcallback %on-text :int ((text :pointer) (len :size) (user :pointer))
  (declare (ignore user))
  (let ((s (foreign-string-to-lisp text :count len :encoding :utf-8)))
    (write-string s *acc*)
    (if *on-delta*
        (handler-case (if (funcall *on-delta* s) 1 0)
          (error (e) (setf *cb-error* e) 1))   ; 回呼丟錯 → 記下、中止、上拋
        0)))

;; 工具呼叫（模型回來要你執行的）：id/name/arguments 皆 NUL 結尾的 const char*，包成 plist 交回呼。
(defcallback %on-tool :int ((call :pointer) (user :pointer))
  (declare (ignore user))
  (if *on-tool*
      (let ((plist (list :id (%opt-str (foreign-slot-value call '(:struct tool-call) 'id))
                          :name (%opt-str (foreign-slot-value call '(:struct tool-call) 'name))
                          :arguments (%opt-str (foreign-slot-value call '(:struct tool-call) 'arguments)))))
        (handler-case (if (funcall *on-tool* plist) 1 0)
          (error (e) (setf *cb-error* e) 1)))
      0))

;; 媒體輸出（模型產出的 audio…）：mime 是 NUL 結尾字串，data 是原始位元組（不保證文字）→ octet vector。
(defcallback %on-media :int ((media :pointer) (user :pointer))
  (declare (ignore user))
  (if *on-media*
      (let* ((mime (%opt-str (foreign-slot-value media '(:struct media-out) 'mime)))
             (data (foreign-slot-value media '(:struct media-out) 'data))
             (len (foreign-slot-value media '(:struct media-out) 'len))
             (bytes (make-array len :element-type '(unsigned-byte 8))))
        (dotimes (i len) (setf (aref bytes i) (mem-ref data :uint8 i)))
        (handler-case (if (funcall *on-media* (list :mime mime :bytes bytes)) 1 0)
          (error (e) (setf *cb-error* e) 1)))
      0))

(defcallback %on-error :void ((msg :pointer) (len :size) (user :pointer))
  (declare (ignore user))
  (setf *err-msg* (foreign-string-to-lisp msg :count len :encoding :utf-8))
  (when *on-error* (ignore-errors (funcall *on-error* *err-msg*))))

(defun ask (prompt &rest args)
  "問 LLM，回完整答案字串。詳見檔頭。
支援 (ask p)／(ask p endpoint)／(ask p :key v …)／(ask p endpoint :key v …)。
（CL 的 &optional 與 &key 不能並存——位置 endpoint 靠 &rest 手動分辨。）
:tools＝(:name … :description … :parameters …) plist 的 list；:on-tool＝(lambda (call) …)，
  call 是 (:id :name :arguments) plist，回真值中止。
:media＝(:url … :mime … :bytes …) plist 的 list（bytes＝(unsigned-byte 8) vector，選填）。
:modalities＝(:name … :config …) plist 的 list。
:on-media＝(lambda (m) …)，m 是 (:mime :bytes) plist（bytes＝octet vector），回真值中止。"
  (let* ((endpoint (or (when (and args (stringp (first args))) (pop args)) ; 位置形式
                       (getf args :endpoint)))                            ; 或 :endpoint 關鍵字
         (system (getf args :system))
         (api-key (getf args :api-key)) (model (getf args :model))
         (timeout-ms (getf args :timeout-ms))
         (temperature (getf args :temperature)) (top-p (getf args :top-p))
         (presence-penalty (getf args :presence-penalty))
         (frequency-penalty (getf args :frequency-penalty))
         (max-tokens (getf args :max-tokens)) (seed (getf args :seed))
         (stream (getf args :stream)) (schema (getf args :schema))
         (on-delta (getf args :on-delta)) (on-error (getf args :on-error))
         (tools (getf args :tools)) (on-tool (getf args :on-tool))
         (media (getf args :media)) (modalities (getf args :modalities))
         (on-media (getf args :on-media))
         (strs '()) (arrs '()))
    (flet ((fstr (s) (if s (let ((p (foreign-string-alloc s :encoding :utf-8)))
                             (push p strs) p)
                         (null-pointer)))
           (falloc (type count) (let ((p (foreign-alloc type :count count))) (push p arrs) p)))
      (unwind-protect
          (with-foreign-objects ((c '(:struct client)) (r '(:struct request))
                                 (h '(:struct handlers)) (sc '(:struct schema)))
            ;; client（全欄位明設）
            (setf (foreign-slot-value c '(:struct client) 'endpoint) (fstr endpoint)
                  (foreign-slot-value c '(:struct client) 'api-key) (fstr api-key)
                  (foreign-slot-value c '(:struct client) 'model) (fstr model)
                  (foreign-slot-value c '(:struct client) 'timeout-ms) (or timeout-ms 0))
            (let ((mask 0))
              (macrolet ((samp (slot bit val ty)
                           `(when ,val
                              (setf (foreign-slot-value c '(:struct client) ',slot) (coerce ,val ',ty))
                              (setf mask (logior mask ,bit)))))
                (setf (foreign-slot-value c '(:struct client) 'temperature) 0.0
                      (foreign-slot-value c '(:struct client) 'top-p) 0.0
                      (foreign-slot-value c '(:struct client) 'presence-penalty) 0.0
                      (foreign-slot-value c '(:struct client) 'frequency-penalty) 0.0
                      (foreign-slot-value c '(:struct client) 'max-tokens) 0
                      (foreign-slot-value c '(:struct client) 'seed) 0)
                (samp temperature +f-temperature+ temperature single-float)
                (samp top-p +f-top-p+ top-p single-float)
                (samp presence-penalty +f-presence+ presence-penalty single-float)
                (samp frequency-penalty +f-frequency+ frequency-penalty single-float)
                (samp max-tokens +f-max-tokens+ max-tokens integer)
                (samp seed +f-seed+ seed integer))
              (setf (foreign-slot-value c '(:struct client) 'field-mask) mask))
            ;; request：tools/media/modalities 是動態長度陣列 → foreign-alloc（跟著 arrs 一起釋放），
            ;; 不比照 with-foreign-objects 的固定結構（那些欄位個數在呼叫前才知道）。
            (let ((tools-n (length tools)) (media-n (length media)) (modalities-n (length modalities))
                  (tools-ptr (null-pointer)) (media-ptr (null-pointer)) (modalities-ptr (null-pointer)))
              (when (plusp tools-n)
                (setf tools-ptr (falloc '(:struct tool-def) tools-n))
                (loop for i from 0 for tool in tools do
                  (let ((slot (mem-aptr tools-ptr '(:struct tool-def) i)))
                    (setf (foreign-slot-value slot '(:struct tool-def) 'name) (fstr (getf tool :name))
                          (foreign-slot-value slot '(:struct tool-def) 'description) (fstr (getf tool :description))
                          (foreign-slot-value slot '(:struct tool-def) 'parameters) (fstr (getf tool :parameters))))))
              (when (plusp media-n)
                (setf media-ptr (falloc '(:struct media-in) media-n))
                (loop for i from 0 for m in media do
                  (let ((slot (mem-aptr media-ptr '(:struct media-in) i))
                        (bytes (getf m :bytes)))
                    (setf (foreign-slot-value slot '(:struct media-in) 'url) (fstr (getf m :url))
                          (foreign-slot-value slot '(:struct media-in) 'mime) (fstr (getf m :mime)))
                    (if bytes
                        (let* ((n (length bytes)) (bp (falloc :uint8 n)))
                          (dotimes (j n) (setf (mem-ref bp :uint8 j) (aref bytes j)))
                          (setf (foreign-slot-value slot '(:struct media-in) 'data) bp
                                (foreign-slot-value slot '(:struct media-in) 'len) n))
                        (setf (foreign-slot-value slot '(:struct media-in) 'data) (null-pointer)
                              (foreign-slot-value slot '(:struct media-in) 'len) 0)))))
              (when (plusp modalities-n)
                (setf modalities-ptr (falloc '(:struct modality) modalities-n))
                (loop for i from 0 for md in modalities do
                  (let ((slot (mem-aptr modalities-ptr '(:struct modality) i)))
                    (setf (foreign-slot-value slot '(:struct modality) 'name) (fstr (getf md :name))
                          (foreign-slot-value slot '(:struct modality) 'config) (fstr (getf md :config))))))
              (setf (foreign-slot-value r '(:struct request) 'prompt) (fstr prompt)
                    (foreign-slot-value r '(:struct request) 'system) (fstr system)
                    (foreign-slot-value r '(:struct request) 'tools) tools-ptr
                    (foreign-slot-value r '(:struct request) 'tools-count) tools-n
                    (foreign-slot-value r '(:struct request) 'media) media-ptr
                    (foreign-slot-value r '(:struct request) 'media-count) media-n
                    (foreign-slot-value r '(:struct request) 'modalities) modalities-ptr
                    (foreign-slot-value r '(:struct request) 'modalities-count) modalities-n
                    (foreign-slot-value r '(:struct request) 'stream) (if stream 1 0)))
            (if schema
                (progn (setf (foreign-slot-value sc '(:struct schema) 'name) (fstr "response")
                             (foreign-slot-value sc '(:struct schema) 'json) (fstr schema))
                       (setf (foreign-slot-value r '(:struct request) 'schema) sc))
                (setf (foreign-slot-value r '(:struct request) 'schema) (null-pointer)))
            ;; handlers（四回呼全裝上；on-tool/on-media 內部靠動態變數判斷有沒有人真的要收）
            (setf (foreign-slot-value h '(:struct handlers) 'on-text) (callback %on-text)
                  (foreign-slot-value h '(:struct handlers) 'text-user) (null-pointer)
                  (foreign-slot-value h '(:struct handlers) 'on-tool) (callback %on-tool)
                  (foreign-slot-value h '(:struct handlers) 'tool-user) (null-pointer)
                  (foreign-slot-value h '(:struct handlers) 'on-media) (callback %on-media)
                  (foreign-slot-value h '(:struct handlers) 'media-user) (null-pointer)
                  (foreign-slot-value h '(:struct handlers) 'on-error) (callback %on-error)
                  (foreign-slot-value h '(:struct handlers) 'error-user) (null-pointer)
                  ;; on-usage 未外露：必須明填 NULL（foreign 記憶體不歸零，殘值會被當函數指標呼叫）
                  (foreign-slot-value h '(:struct handlers) 'on-usage) (null-pointer)
                  (foreign-slot-value h '(:struct handlers) 'usage-user) (null-pointer))
            ;; 呼叫（動態變數餵給 callback）
            (let ((*acc* (make-string-output-stream))
                  (*on-delta* on-delta) (*on-error* on-error)
                  (*on-tool* on-tool) (*on-media* on-media)
                  (*cb-error* nil) (*err-msg* nil))
              (let ((st (%llm-ask (null-pointer) c r h)))
                (cond (*cb-error* (error *cb-error*))
                      ((= st 0) (get-output-stream-string *acc*))          ; LLM_OK
                      ((= st -1) (get-output-stream-string *acc*))         ; 取消 → 回部分
                      (on-error nil)                                        ; 錯誤，但 on-error 已呼叫
                      (t (error 'llm-error :message (or *err-msg* "llm_ask failed")))))))
        (dolist (p strs) (foreign-string-free p))
        (dolist (p arrs) (foreign-free p))))))
