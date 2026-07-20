;;;; cl-lab 核心 —— 純函式 + clingon CLI。
;;;; 一支就示範你最常用的：CLI（clingon）、JSON（jzon）、串列/雜湊表。
(in-package :cl-lab)

;;; ── 純函式 ────────────────────────────────────────────────────────
(defun hello (&optional (who "world"))
  "回傳打招呼字串。who 省略時預設 world。"
  (format nil "Hello, ~a!" who))

(defun greetings (names)
  "把一串名字 map 成招呼串列。list -> list。"
  (mapcar #'hello names))

(defun summarize (names &optional upper)
  "把名字整理成一張雜湊表（string key，方便直接 jzon 成 JSON object）。"
  (let ((norm (if upper (mapcar #'string-upcase names) names))
        (h (make-hash-table :test 'equal)))
    (setf (gethash "count" h)  (length norm)
          (gethash "names" h)  norm
          (gethash "hellos" h) (greetings norm))
    h))

(defun summary-json (names &optional upper)
  "把摘要編成縮排好讀的 JSON 字串。"
  (json:stringify (summarize names upper) :pretty t))

;;; ── CLI（clingon）─────────────────────────────────────────────────
(defun greet-handler (cmd)
  "頂層命令的動作：合併 --name 與位置參數，輸出純文字或 JSON。"
  (let* ((names (clingon:getopt cmd :name))            ; :list -> 串列
         (upper (clingon:getopt cmd :upper))           ; :flag -> t/nil
         (as-json (clingon:getopt cmd :json))
         (args (clingon:command-arguments cmd))        ; 位置參數
         (all (or (append names args) (list "world"))))
    (if as-json
        (format t "~a~%" (summary-json all upper))
        (let ((s (summarize all upper)))
          (format t "共 ~d 位：~%" (gethash "count" s))
          (dolist (g (gethash "hellos" s))
            (format t "  ~a~%" g))))))

(defun cli-command ()
  "定義頂層命令與選項。"
  (clingon:make-command
   :name "cl-lab"
   :description "打招呼 + JSON 小示範"
   :options
   (list
    (clingon:make-option :list  :description "名字，可重複給"
                         :long-name "name" :short-name #\n :key :name)
    (clingon:make-option :flag  :description "名字轉大寫"
                         :long-name "upper" :short-name #\u :key :upper)
    (clingon:make-option :flag  :description "以 JSON 輸出"
                         :long-name "json" :short-name #\j :key :json))
   :handler #'greet-handler))

(defun main ()
  "執行檔進入點（asdf:make 的 entry-point）。"
  (clingon:run (cli-command)))
