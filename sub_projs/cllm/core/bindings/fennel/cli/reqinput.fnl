;; reqinput.fnl — 把 CLI 的請求類旗標驗證／組成內核 ask 的請求輸入（cli.cpp 組請求段）。
;;
;; ⚠ 與 C++ llm 刻意分歧（同 core-py）：--schema/--tool/--modality 的 cfg 收「字面 JSON 文字」（同
;; --system），不再開檔；要吃檔案內容一律靠 shell $(cat s.json)。解 JSON 失敗＝用法錯（退 1）。
;; --image/--media 的三分流取值在 media.fnl。本檔把這些旗標收成 schema／tools／modalities／media
;; 四份輸入，並驗 --media-out 目錄（用 os.execute test -d，POSIX）。內核的 tool.parameters／schema
;; 欄位皆為 JSON 字串，故 parameters 若解出 table 需再 encode 回字串。

(local json (require :dkjson))
(local media (require :media))
(local internal (require :internal))

(fn dir? [path]
  "path 是否為既存目錄（POSIX：靠 shell test -d）。"
  (let [(ok _ code) (os.execute (.. "test -d '" path "'"))]
    (and ok (= code 0))))

(fn build-request-inputs [p]
  "從 parsed 組請求輸入，回 (r EXIT_OK)；驗證失敗回 (nil 退出碼)。"
  (local r {:schema-body nil :tool-defs [] :modality-items [] :media-items []})
  (var bad nil)

  (when (and (= bad nil) p.has-schema) ; 只驗合法；字面原樣交內核嵌入
    (let [(obj _ derr) (json.decode p.schema-text)]
      (if derr
          (do (io.stderr:write "--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）\n")
              (set bad internal.EXIT_USAGE))
          (set r.schema-body p.schema-text))))

  (each [_ spec (ipairs p.tool-specs)] ; 字面 tool def JSON；需 name 與非空 parameters
    (when (= bad nil)
      (let [(td _ derr) (json.decode spec)]
        (if (or derr (not= (type td) "table")) (do (io.stderr:write "--tool 不是合法 JSON（收字面文字非路徑；長內容用 $(cat t.json)）\n") (set bad internal.EXIT_USAGE))
            (or (not td.name) (not td.parameters)) (do (io.stderr:write "--tool 缺 name 或 parameters\n") (set bad internal.EXIT_USAGE))
            (let [params (if (= (type td.parameters) "string") td.parameters (json.encode td.parameters))]
              (table.insert r.tool-defs {:name td.name :description (or td.description "") :parameters params}))))))

  (each [_ spec (ipairs p.modality-specs)] ; 「名」或「名=<字面JSON>」
    (when (= bad nil)
      (let [eqpos (spec:find "=" 1 true)
            name (if eqpos (spec:sub 1 (- eqpos 1)) spec)
            cfg (if eqpos (spec:sub (+ eqpos 1)) nil)]
        (if (= name "") (do (io.stderr:write "--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）\n") (set bad internal.EXIT_USAGE))
            (let [(_ _ derr) (if cfg (json.decode cfg) (values nil nil nil))]
              (if (and cfg derr)
                  (do (io.stderr:write "--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）\n") (set bad internal.EXIT_USAGE))
                  (table.insert r.modality-items (if cfg {:name name :config cfg} {:name name}))))))))

  (each [_ spec (ipairs p.media-specs)] ; 三分流（見 media.fnl）
    (when (= bad nil)
      (let [item (media.build-media-item spec)]
        (if (= item nil) (set bad internal.EXIT_USAGE)
            (table.insert r.media-items item)))))

  (when (and (= bad nil) p.media-out-dir (not (dir? p.media-out-dir)))
    (io.stderr:write (.. "--media-out 不是可用目錄：" p.media-out-dir "\n"))
    (set bad internal.EXIT_USAGE))

  (if bad (values nil bad) (values r internal.EXIT_OK)))

{: build-request-inputs}
