;;;; util.lisp —— lisp-handy 四工具共用碼（shquote / script-dir / run / read-stdin）。
;;;; 由各工具（llme/zhtw/wf/mail）在頂層以 (load .../src/util.lisp) 載入，全放 CL-USER。
;;;; 只用 SBCL 內建：sb-ext:posix-getenv / sb-ext:run-program / 標準檔案 I/O。免 quicklisp。
;;;;
;;;; 編碼：本檔存 UTF-8；SBCL 2.6 預設 external-format 為 :utf-8（見 sb-impl::*default-external-format*），
;;;; argv/檔案 I/O/run-program pipe 皆以 UTF-8 處理（run-program 一律帶 :external-format :utf-8）。

;; 保險：把預設 external-format 釘成 UTF-8（影響之後開的 stream / run-program pipe）。
(setf sb-impl::*default-external-format* :utf-8)

;; %s 空白集合（對齊 Lua 的 %s：space tab newline return formfeed vtab）
(defparameter *ws* (list #\Space #\Tab #\Newline #\Return #\Page (code-char 11)))

(defun getenv-nonempty (k)
  "環境變數；未設或空字串回 nil。"
  (let ((v (sb-ext:posix-getenv k)))
    (if (and v (> (length v) 0)) v nil)))

(defun str-trim (s)
  "去頭尾空白（對齊 Fennel trim ^%s*(.-)%s*$）。"
  (string-trim *ws* s))

;; 單引號包住整串，內部 ' → '\''（吃中文/空白/旗標）。照 Fennel shquote 一字不差。
(defun shquote (s)
  (with-output-to-string (o)
    (write-char #\' o)
    (loop for c across s do
      (if (char= c #\')
          (write-string "'\\''" o)
          (write-char c o)))
    (write-char #\' o)))

;; ── script-dir：從 *load-truename*（呼叫端工具檔、symlink 已解）取所在目錄字串（無尾斜線）。
(defun dir-of (truename)
  (string-right-trim "/" (directory-namestring truename)))

;; ── stdin：只有非 tty（被 pipe/重導）才該讀。偵測用 `test -t 0`（不讀位元組），退出碼 0＝tty。
(defun stdin-piped-p ()
  (let ((p (sb-ext:run-program "test" '("-t" "0")
                               :search t :input t :output nil :error nil :wait t)))
    (not (eql 0 (sb-ext:process-exit-code p)))))

(defun read-all-stdin ()
  "一次讀 stdin 到 EOF（UTF-8）。"
  (with-output-to-string (o)
    (loop for c = (read-char *standard-input* nil nil)
          while c do (write-char c o))))

;; ── 跑子命令：繼承 stdout/stderr（可見/可被測試捕捉），透傳 exit code。
;;    input 給字串時，經 stdin pipe（UTF-8）餵給子程序。:search t 用 PATH 找。
(defun run-exec (prog args &key input)
  (let* ((in (if input (make-string-input-stream input) t))
         (p (sb-ext:run-program prog args
                                :search t :input in :output t :error t
                                :external-format :utf-8 :wait t)))
    (or (sb-ext:process-exit-code p) 0)))

;; ── 跑子命令並捕捉 stdout（給 wf 的 LLM 分類用）。回 (values 輸出字串 exit-code)。
(defun run-capture (prog args)
  (let ((p (sb-ext:run-program prog args
                               :search t :input t :output :stream :error t
                               :external-format :utf-8 :wait t)))
    (values
     (with-output-to-string (o)
       (loop for c = (read-char (sb-ext:process-output p) nil nil)
             while c do (write-char c o)))
     (or (sb-ext:process-exit-code p) 0))))

;; ── 檔案 I/O（UTF-8）。
(defun read-file (path)
  (with-open-file (f path :direction :input :external-format :utf-8
                          :if-does-not-exist nil)
    (when f
      (with-output-to-string (o)
        (loop for c = (read-char f nil nil) while c do (write-char c o))))))

(defun write-file (path body)
  (with-open-file (f path :direction :output :if-exists :supersede
                          :if-does-not-exist :create :external-format :utf-8)
    (write-string body f)))

;; 掃某目錄頂層的 <name>.json / <name>.md（不遞迴）。回 stem 清單（已排序，對齊 ls -1）。
(defun list-stems (dir type &key skip-underscore)
  (let ((res nil))
    (dolist (p (directory (merge-pathnames (concatenate 'string "*." type)
                                           (concatenate 'string (string-right-trim "/" dir) "/"))))
      (let ((name (pathname-name p)))
        (when (and name (> (length name) 0)
                   (not (and skip-underscore (char= (char name 0) #\_))))
          (push name res))))
    (sort res #'string<)))
