# reqinput.janet — 把 CLI 的請求類旗標驗證／組成 llm/ask 的請求輸入（對齊 cli.cpp 組請求段）。
#
# ⚠ 與 C++ llm 刻意分歧（同 core-py）：--schema/--tool/--modality 的 cfg 收「字面 JSON 文字」
# （同 --system），不再開檔；要吃檔案內容一律靠 shell $(cat s.json)。解 JSON 失敗＝用法錯（退 1）。
# --image/--media 的三分流取值在 media.janet。本檔把這些旗標收成 llm/ask 要的 schema／tools／
# modalities／media 四份輸入，並驗 --media-out 目錄。tool 的 :parameters 交 binding 需為 JSON 字串。

(import spork/json)
(import ./internal :as internal)
(import ./media :as media)

(defn- valid-json? [s] (first (protect (json/decode s))))

(defn build-request-inputs [p]
  ``從 parsed 表組請求輸入，回 [inputs nil]；驗證失敗回 [nil 退出碼]。
  inputs＝@{:schema-body :tool-defs :modality-items :media-items}。``
  (def r @{:schema-body nil :tool-defs @[] :modality-items @[] :media-items @[]})
  (var bad nil) # [nil 退出碼]

  (defn fail [msg] (eprin msg "\n") (set bad [nil internal/exit-usage]))

  # --schema：字面 JSON，只驗合法，原樣交 binding 的 :schema
  (when (and (nil? bad) (p :has-schema))
    (if (valid-json? (p :schema-text))
      (put r :schema-body (p :schema-text))
      (fail "--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）")))

  # --tool：字面 tool def JSON；需 name 與非空 parameters（對齊 load_tool_def 語意）
  (each spec (p :tool-specs)
    (when (nil? bad)
      (let [[ok td] (protect (json/decode spec))]
        (cond
          (not ok) (fail "--tool 不是合法 JSON（收字面文字非路徑；長內容用 $(cat t.json)）")
          (or (not (dictionary? td)) (not (get td "name")) (not (get td "parameters")))
          (fail "--tool 缺 name 或 parameters")
          # parameters 是 JSON 物件 → 回編成字串（binding 的 :parameters 需 JSON 字串）
          (array/push (r :tool-defs)
                      {:name (get td "name")
                       :description (get td "description" "")
                       :parameters (string (json/encode (get td "parameters")))})))))

  # --modality：「名」或「名=<字面JSON>」
  (each spec (p :modality-specs)
    (when (nil? bad)
      (def eq-idx (string/find "=" spec))
      (def name (if eq-idx (string/slice spec 0 eq-idx) spec))
      (def cfg (when eq-idx (string/slice spec (inc eq-idx))))
      (cond
        (empty? name) (fail "--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）")
        (and eq-idx (not (valid-json? cfg)))
        (fail "--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）")
        (array/push (r :modality-items) {:name name :config cfg}))))

  # --image/--media：三分流（URL 直通 / .json 描述子直通 / 二進位圖檔讀檔編碼）
  (each spec (p :media-specs)
    (when (nil? bad)
      (def item (media/build-media-item spec))
      (if (nil? item) (set bad [nil internal/exit-usage])
        (array/push (r :media-items) item))))

  # --media-out 目錄檢查
  (when (and (nil? bad) (p :media-out-dir))
    (unless (= :directory (os/stat (p :media-out-dir) :mode))
      (fail (string "--media-out 不是可用目錄：" (p :media-out-dir)))))

  (if bad bad [r nil]))
