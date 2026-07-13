;;; llm.scm — galtxt try_1：s7 Scheme 版統一 LLM 接口（REPL 函式優先，無 streaming）
;;;
;;; 開發循環（playground 哲學）：REPL 一直開著 → 改本檔 → (load "llm.scm") → 直接呼叫。
;;;   (set! *llm-base-url* "http://localhost:1234/v1")   ; 後端用全域變數切（沒 getenv，用變數）
;;;   (llm-entry :prompt "你是一隻貓娘，請回答我問題" :temp 0.1)
;;;   (llm-entry :in "q.txt" :out "a.txt" :sys "你是傲嬌貓娘" :temp 0.8 :top-p 0.9 :max-tokens 256)
;;;
;;; HTTP 走 (system "curl …" #t) 捕捉輸出（MinGW 無 libc，但 system+curl 可用，http/https 通吃）。
;;; 請求用 inlet 表達、s7->json 序列化；回應 json->s7 成 inlet 導航——同像性，零手工 escape。

;; ── JSON：抠自 s7 playground lib/json.scm 的 json->s7 / s7->json，
;;    並替 s7->json 補上 boolean / null（原版沒有，stream:false 會炸）。

(define json->s7
  (let ()
    (define (strlet . args)            ; inlet with keys as strings
      (apply inlet (do ((p args (cddr p))
			(fields ()))
		       ((null? p)
			(reverse! fields))
		     (set! fields (cons (cadr p)
					(cons (symbol (car p))
					      fields))))))
    (let ((jlet (curlet)))
      (lambda (str)
	(do ((p (open-output-string))
	     (len (length str))
	     (i 0 (+ i 1)))
	    ((= i len)
	     (let ((result (eval-string (get-output-string p) jlet)))
	       (close-output-port p)
	       result))
	  (case (str i)
	    ((#\{) (display "(strlet " p))
	    ((#\[) (display "(vector " p))
	    ((#\} #\]) (write-char #\) p))
	    ((#\: #\,) (write-char #\space p))
	    ((#\")
	     (let ((qpos (char-position #\" str (+ i 1))))
	       (if (not qpos)
		   (format *stderr* "no close quote: ~S ~S~%" (substring str 0 i) (substring str i)))
	       (if (char=? (str (- qpos 1)) #\\)
		   (set! qpos (char-position #\" str (+ qpos 1))))
	       (display (substring str i (+ qpos 1)) p)
	       (set! i qpos)))
	    ((#\t)
	     (if (and (< i (- len 3)) (string=? (substring str i (+ i 4)) "true"))
		 (begin (display "#t" p) (set! i (+ i 3)))
		 (format *stderr* "bad entry: ~S~%" (substring str i))))
	    ((#\n)
	     (if (and (< i (- len 3)) (string=? (substring str i (+ i 4)) "null"))
		 (begin (display "()" p) (set! i (+ i 3)))
		 (format *stderr* "bad entry: ~S~%" (substring str i))))
	    ((#\f)
	     (if (and (< i (- len 4)) (string=? (substring str i (+ i 5)) "false"))
		 (begin (display "#f" p) (set! i (+ i 4)))
		 (format *stderr* "bad entry: ~S~%" (substring str i))))
	    (else (write-char (str i) p))))))))

(define* (s7->json obj (port (current-output-port)))
  (case (type-of obj)
    ((integer? float?) (display obj port))
    ((boolean?)        (display (if obj "true" "false") port))   ; ← 補：JSON boolean
    ((null?)           (display "null" port))                    ; ← 補：JSON null
    ((string?)         (write obj port))
    ((vector? float-vector? int-vector? byte-vector?)
     (let ((len (length obj)))
       (if (zero? len)
	   (display "[]" port)
	   (begin
	     (write-char #\[ port)
	     (do ((i 0 (+ i 1)))
		 ((= i (- len 1)) (s7->json (obj i) port) (write-char #\] port))
	       (s7->json (obj i) port)
	       (display ", " port))))))
    ((let?)
     (let ((len (length obj)))
       (if (zero? len)
	   (display "{}" port)
	   (let ((slot-ctr 1))
	     (write-char #\{ port)
	     (for-each (lambda (slot)
			 (write (symbol->string (car slot)) port)
			 (display " : " port)
			 (s7->json (cdr slot) port)
			 (if (< slot-ctr len) (display ", " port) (write-char #\} port))
			 (set! slot-ctr (+ slot-ctr 1)))
		       obj)))))          ; 不 reverse（此 s7 版不准 reverse let；欄位順序對 API 無所謂）
    (else (format *stderr* "s7->json: bad entry: ~S~%" obj))))

(define (json-string obj)              ; s7 物件 → JSON 字串
  (let ((p (open-output-string)))
    (s7->json obj p)
    (get-output-string p)))

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

;; ── 核心：keyword 參數 ＝ schema 的直接投影（之後這份簽章要改由 schema 生成）
(define* (llm-entry (prompt #f) (in #f) (out #f) (sys #f)
		    (model #f) (temp #f) (top-p #f) (top-k #f)
		    (max-tokens #f) (n #f) (seed #f)
		    (presence-penalty #f) (frequency-penalty #f)
		    (base-url #f) (api-key #f))
  ;; 1. 組 prompt：--prompt 本體 ＋（--in 檔接在後面，中間補換行）
  (let* ((body (cond ((and prompt in) (string-append prompt "\n" (slurp in)))
		     (prompt prompt)
		     (in (slurp in))
		     (else (error 'llm-entry "需要 :prompt 或 :in"))))
	 (mdl  (or model *llm-model*))
	 (base (or base-url *llm-base-url*))
	 (key  (or api-key *llm-api-key*))
	 ;; 2. messages（有 :sys 就前置 system role）
	 (user (inlet 'role "user" 'content body))
	 (msgs (if sys (vector (inlet 'role "system" 'content sys) user) (vector user)))
	 ;; 3. 請求 inlet：固定欄位 ＋ 有給才加的取樣參數
	 (req  (inlet 'model mdl 'messages msgs 'stream #f)))
    (when temp              (varlet req 'temperature temp))
    (when top-p             (varlet req 'top_p top-p))
    (when top-k             (varlet req 'top_k top-k))
    (when max-tokens        (varlet req 'max_tokens max-tokens))
    (when n                 (varlet req 'n n))
    (when seed              (varlet req 'seed seed))
    (when presence-penalty  (varlet req 'presence_penalty presence-penalty))
    (when frequency-penalty (varlet req 'frequency_penalty frequency-penalty))
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

(display "llm.scm 已載入。試： (llm-entry :prompt \"hi\" :temp 0.1)")
(newline)
