;;;; cli.lisp — 薄 CLI 外殼：把命令列組成一次 cllm:ask 的發問
;;;; （對齊 core-py/cllm/cli.py，其對齊 core/src/cli.cpp）。
;;;;
;;;; 只做「參數解析 + I/O 接線」，真正的活（組請求／打 HTTP／解回應）全丟給 binding 的 cllm:ask。
;;;; 周邊拆到姊妹檔（皆對齊 C++／core-py 的分檔）：internal＝退出碼／檔案讀取；flags＝反射旗標表
;;;; ＋print-usage；argv＝argv 掃描；config＝三層 config；media＝--media 三分流；reqinput＝請求類
;;;; 旗標組輸入；output＝出口 handlers（Sink）。本檔（對齊 cli.cpp）只留 main 接線。
;;;;
;;;; 流程：(1) 掃描 argv → (2) 定 prompt（位置參數 × 導管 stdin，「-」＝插入點）→ (3) 組 client
;;;; 設定（內建預設 → config → 反射旗標覆寫）→ (4) 組請求輸入 ＋ 呼叫 cllm:ask，output 走 Sink。
;;;; 退出碼：0 成功；1 用法錯；2 請求失敗；130 SIGINT（在 main.lisp 攔）。
(in-package :cllm-cli)

(defun stdin-tty-p ()
  "stdin（fd 0）是否為互動終端（是則不讀，避免卡住）。偵測失敗當作非終端（會去讀）。"
  (handler-case
      (let* ((isatty (find-symbol "UNIX-ISATTY" :sb-unix))
             (r (and isatty (funcall isatty 0))))  ; unix-isatty 回整數（1=tty 0=否），非布林
        (and r (if (integerp r) (plusp r) t)))
    (error () nil)))

(defun slurp-stdin ()
  "整段讀 stdin，去尾端換行（對齊 cli.py 的 sys.stdin.read().rstrip）。"
  (let ((out (make-string-output-stream)))
    (loop for ch = (read-char *standard-input* nil :eof)
          until (eq ch :eof) do (write-char ch out))
    (string-right-trim '(#\Return #\Newline) (get-output-stream-string out))))

(defun coerce-value (raw type)
  "命令列字串 → 型別值。回 (values 值 NIL) 或 (values NIL 型別中文名)。"
  (ecase type
    (:string (values raw nil))
    (:int (handler-case (values (parse-integer raw) nil)
            (error () (values nil "整數"))))
    (:float (multiple-value-bind (v pos)
                (ignore-errors (let ((*read-eval* nil)) (read-from-string raw nil nil)))
              (if (and (realp v) (eql pos (length raw)))
                  (values v nil)
                  (values nil "小數"))))))

(defun assemble-prompt (p)
  "定 prompt（對齊 cli.py step 2）。回 (values PROMPT +exit-ok+)；缺 prompt／「-」但無 stdin 回 (values NIL 碼)。"
  (let* ((parts (parsed-args-prompt-parts p))
         (has-dash (member "-" parts :test #'string=))
         (stdin-text ""))
    (cond ((not (stdin-tty-p)) (setf stdin-text (slurp-stdin)))
          (has-dash
           (format *error-output* "「-」要從 stdin 讀，但 stdin 是互動終端~
                                   ——用導管/檔案餵入（llm --help 看用法）~%")
           (return-from assemble-prompt (values nil +exit-usage+))))
    (let* ((pieces (mapcar (lambda (part) (if (string= part "-") stdin-text part)) parts))
           (prompt (format nil "~{~a~^ ~}" pieces)))
      (when (and (not has-dash) (plusp (length stdin-text)))  ; 沒寫「-」而兩者都有＝prompt＋空行＋stdin
        (setf prompt (if (zerop (length prompt))
                         stdin-text
                         (concatenate 'string prompt (string #\Newline) (string #\Newline) stdin-text))))
      (when (zerop (length prompt))
        (format *error-output* "缺少 prompt：給位置參數或從 stdin 餵入（llm --help 看用法）~%")
        (return-from assemble-prompt (values nil +exit-usage+)))
      (values prompt +exit-ok+))))

(defun build-ask-keyargs (client p r sink)
  "把 client 設定／請求輸入／Sink 回呼組成 cllm:ask 的關鍵字參數 list（未給的欄位傳 NIL＝不送）。"
  (list :endpoint (gethash :endpoint client) :api-key (gethash :api-key client)
        :model (gethash :model client) :timeout-ms (gethash :timeout-ms client)
        :temperature (gethash :temperature client) :top-p (gethash :top-p client)
        :presence-penalty (gethash :presence-penalty client)
        :frequency-penalty (gethash :frequency-penalty client)
        :max-tokens (gethash :max-tokens client) :seed (gethash :seed client)
        :system (when (parsed-args-has-system p) (parsed-args-system-text p))
        :stream (parsed-args-stream p)
        :schema (request-inputs-schema-body r)
        :tools (request-inputs-tool-defs r)
        :media (request-inputs-media-items r)
        :modalities (request-inputs-modality-items r)
        :on-delta (sink-on-delta-fn sink) :on-tool (sink-on-tool-fn sink)
        :on-media (sink-on-media-fn sink) :on-error (sink-on-error-fn sink)))

(defun main (args)
  "CLI 進入點；ARGS 之首為程式名。回退出碼。"
  (multiple-value-bind (p ec) (parse-argv args)
    (unless p (return-from main ec))                                  ; (1) 掃描 argv
    (multiple-value-bind (prompt ec2) (assemble-prompt p)
      (unless prompt (return-from main ec2))                          ; (2) prompt
      (let ((client (make-hash-table :test 'eql)))                    ; (3) client 設定
        (let ((cc (load-config client (parsed-args-has-config p) (parsed-args-config-path p))))
          (unless (= cc +exit-ok+) (return-from main cc)))
        (dolist (row (parsed-args-raw-values p))                      ; 反射旗標覆寫
          (destructuring-bind (kw raw type flag) row
            (multiple-value-bind (val err) (coerce-value raw type)
              (when err
                (format *error-output* "~a 需要~a（給了「~a」）~%" flag err raw)
                (return-from main +exit-usage+))
              (setf (gethash kw client) val))))
        (multiple-value-bind (r ec3) (build-request-inputs p)         ; (4) 請求輸入 ＋ 呼叫內核
          (unless r (return-from main ec3))
          (let* ((sink (make-sink :media-out-dir (parsed-args-media-out-dir p)))
                 (result (apply #'cllm:ask prompt (build-ask-keyargs client p r sink))))
            ;; binding：錯誤分支（給了 on-error）回 NIL；成功回字串、取消回部分字串。
            (when (null result) (setf (sink-had-error sink) t))
            (when (and (sink-wrote-text sink) (not (sink-had-error sink)))
              (write-char #\Newline *standard-output*) (finish-output *standard-output*))
            (cond ((sink-had-error sink) +exit-request+)
                  ((sink-media-err sink) +exit-request+)  ; 媒體落檔失敗＝結果真掉了
                  (t +exit-ok+))))))))
