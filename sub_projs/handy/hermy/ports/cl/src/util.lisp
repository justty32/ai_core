;; util.lisp — hermy CL 版共用碼。各可執行檔先設 *root*（本專案根，解 symlink 用
;; *load-truename*）再 (load) 本檔。HTTP 靠 curl、JSON 靠 jq，全部 shell-out（不引
;; dexador/shasht）。全程 UTF-8：argv/檔名/stdin/stdout/子行程皆不可亂碼。
(setf sb-impl::*default-external-format* :utf-8)

(defun env (name &optional default)
  "posix-getenv，空字串視同未設。"
  (let ((v (sb-ext:posix-getenv name)))
    (if (and v (plusp (length v))) v default)))

(defun env-dir (name)
  "環境變數當目錄 pathname（補尾斜線），未設回 nil。"
  (let ((v (sb-ext:posix-getenv name)))
    (when (and v (plusp (length v)))
      (pathname (if (char= (char v (1- (length v))) #\/) v
                    (concatenate 'string v "/"))))))

(defun skills-dir () (or (env-dir "HERMY_SKILLS_DIR") (merge-pathnames "skills/" *root*)))
(defun memory-dir () (or (env-dir "HERMY_MEMORY_DIR") (merge-pathnames "memory/" *root*)))

(defun trim (s) (string-trim '(#\Space #\Newline #\Tab #\Return) (or s "")))

(defun slurp (&optional (stream *standard-input*))
  "把整個 stream 讀成字串（UTF-8）。"
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
  "shell-out。回傳 (values stdout stderr exit-code)。program 走 PATH 搜尋；UTF-8。"
  (let ((out (make-string-output-stream)) (err (make-string-output-stream)))
    (let ((p (sb-ext:run-program program args
               :input (when input (make-string-input-stream input))
               :output out :error err :search t :external-format :utf-8)))
      (values (get-output-stream-string out) (get-output-stream-string err)
              (sb-ext:process-exit-code p)))))

(defun jq (filter &key (input "null") args)
  "跑 jq -c <args...> <filter>，回傳 stdout（去尾換行）；jq 失敗回 nil。
   raw 輸出於 args 傳 \"-r\"；帶變數用 \"--arg\"/\"--argjson\"。"
  (multiple-value-bind (o e c) (run "jq" (append '("-c") args (list filter)) :input input)
    (declare (ignore e))
    (when (zerop c) (string-right-trim '(#\Newline) o))))

(defun iso-now ()
  "UTC ISO-8601（秒級，無微秒）——如 2026-07-18T12:34:56+00:00。"
  (multiple-value-bind (s mi h d mo y) (decode-universal-time (get-universal-time) 0)
    (format nil "~4,'0d-~2,'0d-~2,'0dT~2,'0d:~2,'0d:~2,'0d+00:00" y mo d h mi s)))
