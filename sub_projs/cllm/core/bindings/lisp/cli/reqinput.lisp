;;;; reqinput.lisp — 把 CLI 的請求類旗標驗證／組成 cllm:ask 的請求輸入
;;;; （對齊 core-py/cllm/reqinput.py，其對齊 core/src/cli.cpp 的組請求段）。
;;;;
;;;; ⚠ 與 C++ llm 刻意分歧（同 core-py）：--schema/--tool/--modality 收「字面 JSON 文字」，
;;;; 不再開檔；要吃檔案內容一律靠 shell $(cat s.json)。解 JSON 失敗＝用法錯（退 1）。
;;;; --image/--media 的三分流取值在 media.lisp（build-media-item）。本檔把這些旗標收成 cllm:ask
;;;; 要的 schema／tools／modalities／media 四份輸入，並驗 --media-out 目錄。
(in-package :cllm-cli)

(defstruct request-inputs
  (schema-body nil) (tool-defs nil) (modality-items nil) (media-items nil))

(defun json-valid-p (text)
  "TEXT 是否為合法 JSON（用 binding 已在用的 shasht 只驗、不留結果）。"
  (handler-case (progn (shasht:read-json text) t) (error () nil)))

(defun build-request-inputs (p)
  "從 PARSED-ARGS 組請求輸入，回 (values REQUEST-INPUTS +exit-ok+)；驗證失敗回 (values NIL 碼)。"
  (let ((r (make-request-inputs)))
    (macrolet ((bail (&body msg) `(progn (format *error-output* ,@msg)
                                         (return-from build-request-inputs (values nil +exit-usage+)))))
      ;; schema：只驗合法；字面原樣交內核嵌入。
      (when (parsed-args-has-schema p)
        (unless (json-valid-p (parsed-args-schema-text p))
          (bail "--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）~%"))
        (setf (request-inputs-schema-body r) (parsed-args-schema-text p)))
      ;; tools：字面 tool def JSON；需 name 與 parameters（對齊 load_tool_def 語意）。
      (dolist (spec (parsed-args-tool-specs p))
        (let (td)
          (handler-case (setf td (shasht:read-json spec))
            (error () (bail "--tool 不是合法 JSON（收字面文字非路徑；長內容用 $(cat t.json)）~%")))
          (unless (and (hash-table-p td) (gethash "name" td)
                       (nth-value 1 (gethash "parameters" td)))
            (bail "--tool 缺 name 或 parameters~%"))
          ;; :parameters 給 binding 要字串 → 把解析出的物件再序列化回 JSON 字串。
          (push (list :name (gethash "name" td)
                      :description (or (gethash "description" td) "")
                      :parameters (with-output-to-string (o)
                                    (shasht:write-json (gethash "parameters" td) o)))
                (request-inputs-tool-defs r))))
      ;; modalities：「名」或「名=<字面JSON>」。
      (dolist (spec (parsed-args-modality-specs p))
        (let* ((pos (position #\= spec))
               (name (if pos (subseq spec 0 pos) spec))
               (cfg (when pos (subseq spec (1+ pos)))))
          (when (zerop (length name))
            (bail "--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）~%"))
          (when (and cfg (not (json-valid-p cfg)))  ; 有 '='：cfg 收字面 JSON，驗合法
            (bail "--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）~%"))
          (push (list :name name :config cfg) (request-inputs-modality-items r))))
      ;; media：三分流（URL 直通 / .json 描述子直通 / 二進位圖檔讀檔編碼）。
      (dolist (spec (parsed-args-media-specs p))
        (let ((item (build-media-item spec)))
          (unless item (return-from build-request-inputs (values nil +exit-usage+)))
          (push item (request-inputs-media-items r))))
      (when (and (parsed-args-media-out-dir p)
                 (not (dir-usable-p (parsed-args-media-out-dir p))))
        (bail "--media-out 不是可用目錄：~a~%" (parsed-args-media-out-dir p)))
      (setf (request-inputs-tool-defs r) (nreverse (request-inputs-tool-defs r))
            (request-inputs-modality-items r) (nreverse (request-inputs-modality-items r))
            (request-inputs-media-items r) (nreverse (request-inputs-media-items r)))
      (values r +exit-ok+))))
