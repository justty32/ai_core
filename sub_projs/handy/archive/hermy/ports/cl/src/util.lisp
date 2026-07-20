;; util.lisp — hermy CL 版的「庫」：所有邏輯都在這裡（package hermy），每個 CLI 只是
;; 「load 本檔 + 呼叫一個 main」的薄殼（<10 行）。庫自己從 *load-truename* 算出 *root*，
;; 故 CLI 不必設。HTTP 靠 curl、JSON 靠 jq，全部 shell-out。全程 UTF-8（argv/檔名/
;; stdin/stdout/子行程皆不亂碼）。
(setf sb-impl::*default-external-format* :utf-8)

(defpackage #:hermy
  (:use #:cl)
  (:export #:orchestrate #:ds-chat #:skills-json #:run-skill #:mem-append))
(in-package #:hermy)

;; *root* = 本庫檔（src/util.lisp）的上一層＝專案根，與哪個 CLI load 它無關。
(defparameter *root* (make-pathname :directory (butlast (pathname-directory *load-truename*))))
(defparameter *lib* (merge-pathnames "lib/" *root*))

;; ── 共用小工具 ──────────────────────────────────────────────────────
(defun env (name &optional default)
  (let ((v (sb-ext:posix-getenv name)))
    (if (and v (plusp (length v))) v default)))

(defun env-dir (name)
  (let ((v (sb-ext:posix-getenv name)))
    (when (and v (plusp (length v)))
      (pathname (if (char= (char v (1- (length v))) #\/) v
                    (concatenate 'string v "/"))))))

(defun skills-dir () (or (env-dir "HERMY_SKILLS_DIR") (merge-pathnames "skills/" *root*)))
(defun memory-dir () (or (env-dir "HERMY_MEMORY_DIR") (merge-pathnames "memory/" *root*)))

(defun trim (s) (string-trim '(#\Space #\Newline #\Tab #\Return) (or s "")))

(defun slurp (&optional (stream *standard-input*))
  (with-output-to-string (o)
    (loop for c = (read-char stream nil :eof) until (eq c :eof) do (write-char c o))))

(defun read-file (path)
  (with-open-file (in path :external-format :utf-8 :if-does-not-exist nil)
    (when in (slurp in))))

(defun emit (s) (write-string (or s "")) (terpri) (finish-output))

(defun die (code fmt &rest args)
  (apply #'format *error-output* fmt args) (terpri *error-output*)
  (sb-ext:exit :code code))

(defun run (program args &key input)
  "shell-out。回 (values stdout stderr exit-code)；PATH 搜尋、UTF-8。"
  (let ((out (make-string-output-stream)) (err (make-string-output-stream)))
    (let ((p (sb-ext:run-program program args
               :input (when input (make-string-input-stream input))
               :output out :error err :search t :external-format :utf-8)))
      (values (get-output-stream-string out) (get-output-stream-string err)
              (sb-ext:process-exit-code p)))))

(defun jq (filter &key (input "null") args)
  "jq -c <args...> <filter> → stdout（去尾換行）；jq 失敗回 nil。"
  (multiple-value-bind (o e c) (run "jq" (append '("-c") args (list filter)) :input input)
    (declare (ignore e))
    (when (zerop c) (string-right-trim '(#\Newline) o))))

(defun iso-now ()
  (multiple-value-bind (s mi h d mo y) (decode-universal-time (get-universal-time) 0)
    (format nil "~4,'0d-~2,'0d-~2,'0dT~2,'0d:~2,'0d:~2,'0d+00:00" y mo d h mi s)))

;; 編排器用的小助手
(defun call (tool &optional args (input ""))
  "呼叫 lib/<tool>：轉發其 stderr，非零退出即結束，回其 stdout。"
  (multiple-value-bind (o e c) (run (namestring (merge-pathnames tool *lib*)) args :input input)
    (let ((es (trim e))) (when (plusp (length es)) (format *error-output* "~a~%" es)))
    (when (/= c 0) (sb-ext:exit :code c))
    o))
(defun mem (rec) (call "mem-append" '() rec))
(defun skills () (let ((s (trim (call "skills-json")))) (if (plusp (length s)) s "[]")))

;; ── 各 CLI 的 main（邏輯全在此，CLI 只呼叫）─────────────────────────
(defun skills-json ()
  "掃 <skills>/*/skill.json → 印 OpenAI function-tools JSON 陣列。"
  (let ((dir (skills-dir)) (objs '()))
    (when (probe-file dir)
      (dolist (d (sort (directory (merge-pathnames "*/" dir))
                       #'string< :key (lambda (p) (car (last (pathname-directory p))))))
        (let* ((name (car (last (pathname-directory d))))
               (meta (merge-pathnames "skill.json" d))
               (run  (merge-pathnames "run" d)))
          (when (and (not (char= (char name 0) #\_)) (probe-file meta) (probe-file run))
            (let ((obj (jq "{type:\"function\",function:{name:(.name // $dn),description:(.description // \"\"),parameters:(.parameters // {type:\"object\",properties:{}})}}"
                           :input (read-file meta) :args (list "--arg" "dn" name))))
              (if obj (push obj objs)
                  (format *error-output* "[skills-json] ~a 的 skill.json 壞了~%" name)))))))
    (emit (format nil "[~{~a~^,~}]" (nreverse objs)))))

(defun run-skill ()
  "stdin=參數 JSON → exec <skills>/<name>/run → stdout 結果（截斷）。"
  (let* ((name (second sb-ext:*posix-argv*))
         (runp (and name (merge-pathnames "run"
                           (merge-pathnames (format nil "~a/" name) (skills-dir))))))
    (if (or (not name) (not (probe-file runp)))
        (progn (emit (format nil "[找不到技能 ~a]" (or name ""))) (sb-ext:exit :code 0))
        (let ((args (slurp)))
          (handler-case
              (multiple-value-bind (o e c) (run (namestring runp) '() :input args)
                (let ((out (trim o)))
                  (when (/= c 0)
                    (let ((es (trim e)))
                      (setf out (format nil "[退出碼 ~a] ~a" c (if (plusp (length es)) es out)))))
                  (emit (if (plusp (length out)) (subseq out 0 (min 8000 (length out)))
                            "（技能無輸出）"))))
            (error (x) (emit (format nil "[技能執行失敗] ~a" x))))))))

(defun mem-append ()
  "stdin=一筆 JSON record → 加 ts → append <memory>/log.ndjson。"
  (let* ((raw (slurp))
         (ts  (iso-now))
         (line (or (jq ". + {ts:$ts}" :input (if (plusp (length (trim raw))) raw "{}")
                       :args (list "--arg" "ts" ts))
                   (jq "{raw:$raw, ts:$ts}" :args (list "--arg" "raw" raw "--arg" "ts" ts))))
         (mdir (memory-dir)))
    (ensure-directories-exist mdir)
    (with-open-file (f (merge-pathnames "log.ndjson" mdir)
                       :direction :output :if-exists :append
                       :if-does-not-exist :create :external-format :utf-8)
      (write-line line f))))

(defun ds-chat ()
  "stdin={messages,tools} → 一次 DeepSeek 呼叫 → stdout 回應 message JSON。"
  (let ((key (env "DEEPSEEK_API_KEY")))
    (unless key (die 2 "[ds-chat] 未設 DEEPSEEK_API_KEY"))
    (let* ((model (env "HERMY_MODEL" "deepseek-chat"))
           (endpoint (env "HERMY_ENDPOINT" "https://api.deepseek.com/v1/chat/completions"))
           (req (trim (slurp)))
           (body (jq "{model:$model, messages:(.messages // [])} + (if ((.tools // [])|length)>0 then {tools:.tools, tool_choice:\"auto\"} else {} end)"
                     :input (if (plusp (length req)) req "{}") :args (list "--arg" "model" model))))
      (unless body (die 1 "[ds-chat] 請求 JSON 解析失敗"))
      (multiple-value-bind (out err code)
          (run "curl" (list "-sS" "-X" "POST" endpoint
                            "-H" "Content-Type: application/json"
                            "-H" (format nil "Authorization: Bearer ~a" key)
                            "--data-binary" "@-" "-w" (format nil "~%%{http_code}"))
               :input body)
        (when (/= code 0)
          (format *error-output* "[ds-chat] 請求失敗：curl ~a ~a~%" code (trim err))
          (sb-ext:exit :code 1))
        (let* ((nl (position #\Newline out :from-end t))
               (http (if nl (subseq out (1+ nl)) out))
               (resp (if nl (subseq out 0 nl) "")))
          (if (and (plusp (length http)) (char= (char http 0) #\2))
              (let ((msg (jq ".choices[0].message" :input resp)))
                (if msg (emit msg)
                    (die 1 "[ds-chat] 回應無 message：~a" (subseq resp 0 (min 500 (length resp))))))
              (progn
                (format *error-output* "[ds-chat] HTTP ~a: ~a~%" http
                        (subseq resp 0 (min 500 (length resp))))
                (sb-ext:exit :code 1))))))))

(defun orchestrate ()
  "編排器：skills-json → 迴圈{ ds-chat → tool_calls 逐個 run-skill → 回饋 } → 答。"
  (let ((task (trim (format nil "~{~a~^ ~}" (cdr sb-ext:*posix-argv*)))))
    (when (and (zerop (length task)) (zerop (sb-unix:unix-isatty 0)))
      (setf task (trim (slurp))))
    (when (zerop (length task))
      (die 2 "用法：hermy <任務...>   或   echo ... | hermy"))
    (let* ((system (trim (read-file (merge-pathnames "prompt/system.md" *root*))))
           (max (or (parse-integer (env "HERMY_MAX_STEPS" "12") :junk-allowed t) 12))
           (tools (skills))
           (messages (jq "[{role:\"system\",content:$sys},{role:\"user\",content:$task}]"
                         :args (list "--arg" "sys" system "--arg" "task" task))))
      (format *error-output* "[hermy] 載入 ~a 個技能~%" (jq "length" :input tools))
      (mem (jq "{role:\"user\",content:$task}" :args (list "--arg" "task" task)))
      (block loop
        (dotimes (i max)
          (let* ((payload (jq "{messages:., tools:$t}" :input messages :args (list "--argjson" "t" tools)))
                 (msg (trim (call "ds-chat" '() payload))))
            (setf messages (jq ". + [$msg]" :input messages :args (list "--argjson" "msg" msg)))
            (let ((n (or (parse-integer (jq "(.tool_calls // []) | length" :input msg) :junk-allowed t) 0)))
              (when (zerop n)
                (let ((content (jq ".content // \"\"" :input msg :args '("-r"))))
                  (emit content)
                  (mem (jq "{role:\"assistant\",content:$c}" :args (list "--arg" "c" content)))
                  (return-from loop)))
              (dotimes (k n)
                (let* ((c   (jq (format nil ".tool_calls[~a]" k) :input msg))
                       (fn  (jq ".function.name" :input c :args '("-r")))
                       (a   (let ((x (jq ".function.arguments // \"{}\"" :input c :args '("-r"))))
                              (if (plusp (length x)) x "{}")))
                       (id  (jq ".id" :input c :args '("-r"))))
                  (format *error-output* "[hermy] → 技能 ~a ~a~%" fn a)
                  (let* ((result (trim (call "run-skill" (list fn) a)))
                         (toolmsg (jq "{role:\"tool\",tool_call_id:$id,content:$r}"
                                      :args (list "--arg" "id" id "--arg" "r" result))))
                    (when (string= fn "create_skill") (setf tools (skills)))
                    (setf messages (jq ". + [$msg]" :input messages :args (list "--argjson" "msg" toolmsg)))
                    (mem (jq "{role:\"tool\",name:$n,result:$r}"
                             :args (list "--arg" "n" fn "--arg" "r"
                                         (subseq result 0 (min 2000 (length result))))))))))))
        (format *error-output* "[hermy] 到達步數上限 ~a~%" max)))))
