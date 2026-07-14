;;; llm.scm — galtxt try_1：s7 Scheme 版統一 LLM 接口（REPL 函式優先，無 streaming）
;;;
;;; 開發循環（playground 哲學）：REPL 一直開著 → 改本檔 → (load "llm.scm") → 直接呼叫。
;;;   (set! *llm-base-url* "http://localhost:1234/v1")   ; 後端用全域變數切（沒 getenv，用變數）
;;;   (llm-entry :prompt "你是一隻貓娘，請回答我問題" :temp 0.1)
;;;   (llm-entry :in "q.txt" :out "a.txt" :sys "你是傲嬌貓娘" :temp 0.8 :top-p 0.9 :max-tokens 256)
;;;
;;; HTTP 走 (system "curl …" #t) 捕捉輸出（MinGW 無 libc，但 system+curl 可用，http/https 通吃）。
;;; 請求用 inlet 表達、s7->json 序列化；回應 json->s7 成 inlet 導航——同像性，零手工 escape。
;;;
;;; ★ 參數單一真相源＝下面的 *llm-schema*：由它「生成」llm-entry 的 keyword 簽章、
;;;   並在 runtime 驅動取樣參數的塞入。新增一個參數＝schema 加一行，別處零改動；
;;;   日後 --flag CLI 也由同一張表生成（見「argv host」那條線）。

;; ── JSON：json->s7 / s7->json / json-string 已抽到 json.scm（純機械搬移，邏輯不變），這裡只 load。
(load "json.scm")   ; json->s7 / s7->json / json-string

;; ── 後端設定（沒 getenv → 用全域變數，REPL 裡 set! 即可切）
(define *llm-base-url* "http://localhost:1234/v1")   ; 到 /v1 為止
(define *llm-model*    "local-model")                ; LM Studio 用當前載入模型；雲端填真實 id
(define *llm-api-key*  "")                            ; 有值才加 Authorization

;; ── 小工具
(define (slurp path)                   ; 讀整個檔成字串（byte-wise，UTF-8 原樣保留）
  (call-with-input-file path
    (lambda (p)
      (let loop ((cs ()))
	(let ((c (read-char p)))
	  (if (eof-object? c) (list->string (reverse cs)) (loop (cons c cs))))))))

(define (spit path str)                ; 字串寫檔
  (call-with-output-file path (lambda (p) (display str p))))

(define *req-file* "galtxt_llm_req.json")   ; 暫存請求 body（避開命令列引號地獄）

;; ── ★ 參數 schema：唯一真相源
;;    每列＝(scheme名  json鍵-或-ctrl)
;;      ctrl        ＝控制參數（prompt/in/out/sys/model/base-url/api-key），本體流程手動處理
;;      其它(符號)  ＝直接塞進 OpenAI 請求的 JSON 欄位鍵；「有給才塞」，沒給讓後端用自己預設
;;    要加參數（例如 stop、logit_bias）：這裡加一行即可，llm-entry 簽章與塞入邏輯自動跟上。
(define *llm-schema*
  '((prompt            ctrl)
    (in                ctrl)
    (out               ctrl)
    (sys               ctrl)
    (model             ctrl)
    (base-url          ctrl)
    (api-key           ctrl)
    (temp              temperature)
    (top-p             top_p)
    (top-k             top_k)
    (max-tokens        max_tokens)
    (n                 n)
    (seed              seed)
    (presence-penalty  presence_penalty)
    (frequency-penalty frequency_penalty)))

(define (schema-ctrl? entry) (eq? (cadr entry) 'ctrl))

;; ── 實作本體：吃一個「已收齊所有 keyword 值的 inlet」a，(a '參數名) 取值（沒給＝#f）。
;;    取樣參數不再手寫——直接 for-each 迭代 *llm-schema*。
(define (llm-entry-impl a)
  (let* ((prompt   (a 'prompt))
	 (in       (a 'in))
	 (out      (a 'out))
	 (sys      (a 'sys))
	 (model    (a 'model))
	 (base-url (a 'base-url))
	 (api-key  (a 'api-key))
	 ;; 1. 組 prompt：--prompt 本體 ＋（--in 檔接在後面，中間補換行）
	 (body (cond ((and prompt in) (string-append prompt "\n" (slurp in)))
		     (prompt prompt)
		     (in (slurp in))
		     (else (error 'llm-entry "需要 :prompt 或 :in"))))
	 (mdl  (or model *llm-model*))
	 (base (or base-url *llm-base-url*))
	 (key  (or api-key *llm-api-key*))
	 ;; 2. messages（有 :sys 就前置 system role）
	 (user (inlet 'role "user" 'content body))
	 (msgs (if sys (vector (inlet 'role "system" 'content sys) user) (vector user)))
	 ;; 3. 請求 inlet：固定欄位 ＋（下方）schema 驅動的取樣參數
	 (req  (inlet 'model mdl 'messages msgs 'stream #f)))
    ;; ★ 取樣參數：迭代 schema，非 ctrl 且有給才塞進 req（json 鍵＝schema 第二欄）
    (for-each (lambda (entry)
		(unless (schema-ctrl? entry)
		  (let ((v (a (car entry))))
		    (when v (varlet req (cadr entry) v)))))
	      *llm-schema*)
    ;; 4. 寫請求檔 → curl → 捕捉回應
    (spit *req-file* (json-string req))
    (let* ((auth (if (> (length key) 0)
		     (string-append " -H \"Authorization: Bearer " key "\"") ""))
	   (cmd  (string-append "curl -sS \"" base "/chat/completions\""
				" -H \"Content-Type: application/json\"" auth
				" --data-binary @" *req-file* " 2>&1"))
	   (resp (system cmd #t)))
      ;; 5. 解析 choices[0].message.content
      (let ((content
	     (catch #t
	       (lambda ()
		 (let* ((j (json->s7 resp))
			(c0 ((j 'choices) 0)))
		   ((c0 'message) 'content)))
	       (lambda args #f))))
	(if (not content)
	    (begin (format *stderr* "llm-entry: 回應無 choices[0].message.content。原始回應：~%~A~%" resp) #f)
	    (begin
	      ;; token 計量（有才印）
	      (catch #t (lambda ()
			  (let ((toks ((json->s7 resp) 'usage)))
			    (when (let? toks) (format *stderr* "[meter] total_tokens=~A~%" (toks 'total_tokens)))))
		     (lambda args #f))
	      (if out (begin (spit out content) out) content)))))))

;; ── ★ 由 schema「生成」llm-entry 的 define* 簽章（消滅手寫參數列）。
;;    生成物等價於：
;;      (define* (llm-entry (prompt #f) (in #f) … (frequency-penalty #f))
;;        (llm-entry-impl (inlet 'prompt prompt 'in in … 'frequency-penalty frequency-penalty)))
;;    ——薄殼只負責「收齊 keyword 值打包成 inlet」，真正邏輯全在 llm-entry-impl。
(define (make-llm-entry! schema)
  (let ((sig   (map (lambda (e) (list (car e) #f)) schema))                 ; ((prompt #f) …)
	(pairs (apply append
		      (map (lambda (e) (list (list 'quote (car e)) (car e)))   ; ('prompt prompt …)
			   schema))))
    (eval `(define* (llm-entry ,@sig)
	     (llm-entry-impl (inlet ,@pairs)))
	  (rootlet))))

(make-llm-entry! *llm-schema*)

(display "llm.scm 已載入。試： (llm-entry :prompt \"hi\" :temp 0.1)")
(newline)
