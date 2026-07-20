;; util.scm — hermy s7 Scheme 版的「庫」：所有邏輯都在這裡，每個 CLI 只是一支 shell shim
;; （定位自身→source env.sh→exec llm-s7 本檔 <main名> "$@"）。s7 拿不到自身 script 路徑，
;; 故 shim 用 env HERMY_ROOT 傳入專案根。dispatch 靠 (car *argv*)＝main 名、(cdr *argv*)＝真參數。
;; HTTP 靠 curl、JSON 靠 jq，全部 shell-out（s7 無原生 json）。全程 UTF-8（中文/emoji 不亂碼）。
;;
;; s7 慣例（見 ~/code/cllm-lab/s7/play.scm）：
;;  - (system cmd #t) 回傳 cmd 的 stdout 字串；(system "cat > f") 可把「本行程 stdin」寫進檔。
;;  - s7 的 Scheme 層預設 input port 沒接到行程 stdin，故讀自身 stdin 一律經 (system "cat > f")。

(define main-name (if (pair? *argv*) (car *argv*) ""))
(define argl      (if (pair? *argv*) (cdr *argv*) '()))
(define root      (let ((v (getenv "HERMY_ROOT"))) (if (and v (> (string-length v) 0)) v ".")))

;; ── 共用小工具 ──────────────────────────────────────────────────────
(define (env name default)
  (let ((v (getenv name))) (if (and v (> (string-length v) 0)) v default)))

(define (str-replace s old new)
  ;; 把 s 中所有 old（字串）換成 new；old 非空。
  (let ((olen (string-length old)) (slen (string-length s)))
    (let loop ((i 0) (acc ""))
      (cond ((> (+ i olen) slen)
             (string-append acc (substring s i slen)))
            ((string=? (substring s i (+ i olen)) old)
             (loop (+ i olen) (string-append acc new)))
            (else
             (loop (+ i 1) (string-append acc (substring s i (+ i 1)))))))))

(define (shquote s)
  ;; POSIX 單引號包裹，內部單引號轉 '\'' 。
  (string-append "'" (str-replace (if (string? s) s (object->string s)) "'" "'\\''") "'"))

(define (trim s)
  ;; 去頭尾空白（space/tab/newline/return）。
  (if (not (string? s)) ""
      (let ((n (string-length s)))
        (let ((a (let loop ((i 0))
                   (if (and (< i n) (memv (string-ref s i) '(#\space #\tab #\newline #\return)))
                       (loop (+ i 1)) i)))
              (b (let loop ((i n))
                   (if (and (> i 0) (memv (string-ref s (- i 1)) '(#\space #\tab #\newline #\return)))
                       (loop (- i 1)) i))))
          (if (< a b) (substring s a b) "")))))

(define (rtrim-nl s)
  ;; 只去尾端換行（對齊 jq -c 輸出）。
  (let loop ((i (string-length s)))
    (if (and (> i 0) (memv (string-ref s (- i 1)) '(#\newline #\return)))
        (loop (- i 1)) (substring s 0 i))))

(define (mktemp) (trim (system "mktemp" #t)))

(define (sh cmd) (system cmd #t))        ; 回 stdout 字串（stderr 直通終端）
(define (sh! cmd) (system cmd))          ; 只求副作用

(define (slurp-file path)
  (if (> (string-length (trim path)) 0)
      (sh (string-append "cat " (shquote path) " 2>/dev/null"))
      ""))

(define (write-file path s)
  (call-with-output-file path (lambda (p) (display (if (string? s) s "") p))))

(define (file-exists? path)
  (= 0 (car (run-capture (string-append "test -e " (shquote path))))))

(define (emit s) (display (if (string? s) s "")) (newline))
(define (eprint s) (display s (current-error-port)) (newline (current-error-port)))
(define (die code msg) (eprint msg) (exit code))

;; run-capture：跑 shell cmd，stderr 直通終端；回 (cons exit-code stdout)。
(define (run-capture cmd)
  (let* ((outf (mktemp))
         (ec   (trim (sh (string-append "{ " cmd " ; } > " (shquote outf) " ; echo $?"))))
         (out  (slurp-file outf)))
    (sh! (string-append "rm -f " (shquote outf)))
    (cons (or (string->number ec) 1) out)))

;; run-capture2：另捕 stderr；回 (list exit-code stdout stderr)。
(define (run-capture2 cmd)
  (let* ((outf (mktemp)) (errf (mktemp))
         (ec   (trim (sh (string-append "{ " cmd " ; } > " (shquote outf)
                                        " 2> " (shquote errf) " ; echo $?"))))
         (out  (slurp-file outf)) (err (slurp-file errf)))
    (sh! (string-append "rm -f " (shquote outf) " " (shquote errf)))
    (list (or (string->number ec) 1) out err)))

;; 讀「本行程 stdin」→ 字串（s7 預設 input port 沒接 stdin，改用 cat）。
(define (read-stdin)
  (let* ((f (mktemp)) (_ (sh! (string-append "cat > " (shquote f))))
         (s (slurp-file f)))
    (sh! (string-append "rm -f " (shquote f)))
    s))

;; jq：input 字串寫暫存檔當 jq 輸入；extra＝額外旗標/引數（如 '("--arg" "k" "v")、'("-r")）。
;; 回 stdout（去尾換行）；jq 非零退出回 #f。大陣列走 input（避開 argv 128KB 限制）。
(define (jq input extra filter)
  (let* ((inf (mktemp)) (_ (write-file inf (if (string? input) input "")))
         (cmd (string-append "jq -c "
                             (apply string-append (map (lambda (a) (string-append (shquote a) " ")) extra))
                             (shquote filter) " " (shquote inf)))
         (r (run-capture cmd)))
    (sh! (string-append "rm -f " (shquote inf)))
    (if (= (car r) 0) (rtrim-nl (cdr r)) #f)))

(define (skills-dir) (env "HERMY_SKILLS_DIR" (string-append root "/skills")))
(define (memory-dir) (env "HERMY_MEMORY_DIR" (string-append root "/memory")))

(define (iso-now) (trim (sh "date -u +%Y-%m-%dT%H:%M:%S+00:00")))

;; 把換行分隔的字串切成 list（去空行）。
(define (lines s)
  (let ((n (string-length s)))
    (let loop ((i 0) (start 0) (acc '()))
      (cond ((>= i n)
             (reverse (if (> i start) (cons (substring s start i) acc) acc)))
            ((char=? (string-ref s i) #\newline)
             (loop (+ i 1) (+ i 1) (if (> i start) (cons (substring s start i) acc) acc)))
            (else (loop (+ i 1) start acc))))))

;; ── 編排器用的小助手：呼叫 root/lib/<tool>，input 走 stdin（暫存檔），stderr 直通 ─────
(define (call-tool tool args input)
  (let* ((inf (mktemp)) (_ (write-file inf (if (string? input) input "")))
         (cmd (string-append (shquote (string-append root "/lib/" tool))
                             (apply string-append (map (lambda (a) (string-append " " (shquote a))) args))
                             " < " (shquote inf)))
         (r (run-capture cmd)))
    (sh! (string-append "rm -f " (shquote inf)))
    (when (not (= (car r) 0)) (exit (car r)))
    (cdr r)))

(define (mem rec) (call-tool "mem-append" '() rec))
(define (skills) (let ((s (trim (call-tool "skills-json" '() "")))) (if (> (string-length s) 0) s "[]")))

;; ── 各 CLI 的 main（邏輯全在此）─────────────────────────────────────
(define (main-skills-json)
  ;; 掃 <skills>/*/skill.json → 印 OpenAI function-tools JSON 陣列。
  (let* ((dir (skills-dir))
         (names (if (file-exists? dir)
                    (lines (sh (string-append "ls -1 " (shquote dir) " 2>/dev/null | sort")))
                    '()))
         (objs '()))
    (for-each
     (lambda (name)
       (let* ((sub  (string-append dir "/" name))
              (meta (string-append sub "/skill.json"))
              (run  (string-append sub "/run")))
         (when (and (> (string-length name) 0)
                    (not (char=? (string-ref name 0) #\_))
                    (file-exists? meta) (file-exists? run))
           (let ((obj (jq (slurp-file meta) (list "--arg" "dn" name)
                          "{type:\"function\",function:{name:(.name // $dn),description:(.description // \"\"),parameters:(.parameters // {type:\"object\",properties:{}})}}")))
             (if obj (set! objs (cons obj objs))
                 (eprint (string-append "[skills-json] " name " 的 skill.json 壞了")))))))
     names)
    (let ((rev (reverse objs)))
      (emit (string-append "[" (if (null? rev) ""
                                   (let loop ((l rev) (acc ""))
                                     (if (null? (cdr l)) (string-append acc (car l))
                                         (loop (cdr l) (string-append acc (car l) ",")))))
                           "]")))))

(define (main-run-skill)
  ;; <name>：stdin=參數 JSON → exec <skills>/<name>/run → stdout 結果（截斷保護）。
  (let* ((name (if (pair? argl) (car argl) ""))
         (runp (if (> (string-length name) 0) (string-append (skills-dir) "/" name "/run") "")))
    (if (or (= (string-length name) 0) (not (file-exists? runp)))
        (begin (emit (string-append "[找不到技能 " name "]")) (exit 0))
        (let* ((args (read-stdin))
               (inf  (mktemp)) (_ (write-file inf args))
               (r    (run-capture2 (string-append (shquote runp) " < " (shquote inf)))))
          (sh! (string-append "rm -f " (shquote inf)))
          (let* ((code (car r)) (out0 (trim (cadr r))) (err0 (trim (caddr r)))
                 (out (if (not (= code 0))
                          (string-append "[退出碼 " (number->string code) "] "
                                         (if (> (string-length err0) 0) err0 out0))
                          out0)))
            (emit (if (> (string-length out) 0)
                      (substring out 0 (min 8000 (string-length out)))
                      "（技能無輸出）")))))))

(define (main-mem-append)
  ;; stdin=一筆 JSON record → 加 ts → append <memory>/log.ndjson。
  (let* ((raw (read-stdin)) (ts (iso-now))
         (line (or (jq (if (> (string-length (trim raw)) 0) raw "{}")
                       (list "--arg" "ts" ts) ". + {ts:$ts}")
                   (jq "null" (list "--arg" "raw" raw "--arg" "ts" ts) "{raw:$raw, ts:$ts}")))
         (mdir (memory-dir)))
    (sh! (string-append "mkdir -p " (shquote mdir)))
    (sh! (string-append "printf '%s\\n' " (shquote line) " >> " (shquote (string-append mdir "/log.ndjson"))))))

(define (main-ds-chat)
  ;; stdin={messages,tools} → 一次 DeepSeek 呼叫（curl）→ stdout 回應 message JSON。
  (let ((key (env "DEEPSEEK_API_KEY" "")))
    (when (= (string-length key) 0) (die 2 "[ds-chat] 未設 DEEPSEEK_API_KEY"))
    (let* ((model (env "HERMY_MODEL" "deepseek-chat"))
           (endpoint (env "HERMY_ENDPOINT" "https://api.deepseek.com/v1/chat/completions"))
           (req (trim (read-stdin)))
           (body (jq (if (> (string-length req) 0) req "{}") (list "--arg" "model" model)
                     "{model:$model, messages:(.messages // [])} + (if ((.tools // [])|length)>0 then {tools:.tools, tool_choice:\"auto\"} else {} end)")))
      (when (not body) (die 1 "[ds-chat] 請求 JSON 解析失敗"))
      (let* ((bodyf (mktemp)) (_ (write-file bodyf body))
             (cmd (string-append "curl -sS -X POST " (shquote endpoint)
                                 " -H " (shquote "Content-Type: application/json")
                                 " -H " (shquote (string-append "Authorization: Bearer " key))
                                 " --data-binary " (shquote (string-append "@" bodyf))
                                 " -w " (shquote "\n%{http_code}")))
             (r (run-capture2 cmd)))
        (sh! (string-append "rm -f " (shquote bodyf)))
        (let ((code (car r)) (out (cadr r)) (err (caddr r)))
          (when (not (= code 0))
            (die 1 (string-append "[ds-chat] 請求失敗：curl " (number->string code) " " (trim err))))
          (let* ((nl (let loop ((i (- (string-length out) 1)))
                       (cond ((< i 0) #f)
                             ((char=? (string-ref out i) #\newline) i)
                             (else (loop (- i 1))))))
                 (http (if nl (substring out (+ nl 1) (string-length out)) out))
                 (resp (if nl (substring out 0 nl) "")))
            (if (and (> (string-length http) 0) (char=? (string-ref http 0) #\2))
                (let ((msg (jq resp '() ".choices[0].message")))
                  (if msg (emit msg)
                      (die 1 (string-append "[ds-chat] 回應無 message："
                                            (substring resp 0 (min 500 (string-length resp)))))))
                (die 1 (string-append "[ds-chat] HTTP " http ": "
                                      (substring resp 0 (min 500 (string-length resp))))))))))))

(define (stdin-not-tty?) (not (= 0 (car (run-capture "test -t 0")))))

(define (main-orchestrate)
  ;; 編排器：skills-json → 迴圈{ ds-chat → tool_calls 逐個 run-skill → 回饋 } → 答。
  (let ((task (trim (apply string-append
                           (let loop ((l argl) (acc '()))
                             (cond ((null? l) (reverse acc))
                                   ((null? (cdr l)) (reverse (cons (car l) acc)))
                                   (else (loop (cdr l) (cons " " (cons (car l) acc))))))))))
    (when (and (= (string-length task) 0) (stdin-not-tty?))
      (set! task (trim (read-stdin))))
    (when (= (string-length task) 0)
      (die 2 "用法：hermy <任務...>   或   echo ... | hermy"))
    (let* ((system-p (trim (slurp-file (string-append root "/prompt/system.md"))))
           (max (or (string->number (env "HERMY_MAX_STEPS" "12")) 12))
           (tools (skills))
           (messages (jq "null" (list "--arg" "sys" system-p "--arg" "task" task)
                         "[{role:\"system\",content:$sys},{role:\"user\",content:$task}]")))
      (eprint (string-append "[hermy] 載入 " (or (jq tools '() "length") "0") " 個技能"))
      (mem (jq "null" (list "--arg" "task" task) "{role:\"user\",content:$task}"))
      (call-with-exit
       (lambda (return)
         (let step ((i 0))
           (when (< i max)
             (let* ((payload (jq messages (list "--argjson" "t" tools) "{messages:., tools:$t}"))
                    (msg (trim (call-tool "ds-chat" '() payload))))
               (set! messages (jq messages (list "--argjson" "msg" msg) ". + [$msg]"))
               (let ((n (or (string->number (jq msg '() "(.tool_calls // []) | length")) 0)))
                 (if (= n 0)
                     (let ((content (jq msg '("-r") ".content // \"\"")))
                       (emit content)
                       (mem (jq "null" (list "--arg" "c" content) "{role:\"assistant\",content:$c}"))
                       (return #t))
                     (begin
                       (let tc ((k 0))
                         (when (< k n)
                           (let* ((c  (jq msg '() (string-append ".tool_calls[" (number->string k) "]")))
                                  (fn (jq c '("-r") ".function.name"))
                                  (a  (let ((x (jq c '("-r") ".function.arguments // \"{}\"")))
                                        (if (and x (> (string-length x) 0)) x "{}")))
                                  (id (jq c '("-r") ".id")))
                             (eprint (string-append "[hermy] → 技能 " fn " " a))
                             (let* ((result (trim (call-tool "run-skill" (list fn) a)))
                                    (toolmsg (jq "null" (list "--arg" "id" id "--arg" "r" result)
                                                 "{role:\"tool\",tool_call_id:$id,content:$r}")))
                               (when (string=? fn "create_skill") (set! tools (skills)))
                               (set! messages (jq messages (list "--argjson" "msg" toolmsg) ". + [$msg]"))
                               (mem (jq "null" (list "--arg" "n" fn "--arg" "r"
                                                     (substring result 0 (min 2000 (string-length result))))
                                        "{role:\"tool\",name:$n,result:$r}")))
                             (tc (+ k 1)))))
                       (step (+ i 1)))))))
           (when (>= i max)
             (eprint (string-append "[hermy] 到達步數上限 " (number->string max))))))))))

;; ── dispatch ────────────────────────────────────────────────────────
(cond ((string=? main-name "skills-json") (main-skills-json))
      ((string=? main-name "ds-chat")     (main-ds-chat))
      ((string=? main-name "run-skill")   (main-run-skill))
      ((string=? main-name "mem-append")  (main-mem-append))
      ((string=? main-name "orchestrate") (main-orchestrate))
      (else (die 2 (string-append "[util.scm] 未知 main：" main-name))))
