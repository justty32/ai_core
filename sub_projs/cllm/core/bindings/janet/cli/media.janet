# media.janet — --image/--media 的 MIME 對照與三分流取值。
#
# mime-of／ext-of 對齊 cli_config.cpp 的同名對照表。build-media-item 是薄 CLI 特有的取值分流：
# 讓 --media 除了讀二進位圖檔，也能吃「已編碼」形式（data:/http URL、或 .json 描述子），省掉每次
# 重算 base64。回傳的 item 交 binding 的 llm/ask（:url / :mime / :bytes 三欄，:bytes 為原始位元組）。

(import spork/json)
(import spork/base64)
(import ./internal :as internal)

(defn mime-of [path]
  ``副檔名 → MIME（對齊 cli_config.cpp 的 mime_of）。``
  (def ext (string/ascii-lower (last (string/split "." path))))
  (get {"png" "image/png" "jpg" "image/jpeg" "jpeg" "image/jpeg"
        "gif" "image/gif" "webp" "image/webp" "wav" "audio/wav"
        "mp3" "audio/mpeg"} ext "application/octet-stream"))

(defn ext-of [mime]
  ``MIME → 落檔副檔名（對齊 cli_config.cpp 的 ext_of）。``
  (get {"image/png" "png" "image/jpeg" "jpg" "image/gif" "gif"
        "image/webp" "webp" "audio/wav" "wav" "audio/mpeg" "mp3"} mime "bin"))

(defn build-media-item [spec]
  ``--image/--media 一個值 → llm/ask 的 media item（三分流）。成功回 struct、失敗印 stderr 回 nil。

  1. data:／http(s):// URL → 直接當 :url 送、不編碼（帶 url 就不帶 bytes）。
  2. .json 副檔名（用副檔名判定、不嗅探內容）→ 讀該檔 json/decode，當「預先算好的 media 描述子」
     直通、不重算 base64。接受兩形：{"url": "data:…"} 或 {"mime": "…", "data": "<base64>"}。
  3. 其餘（二進位圖檔路徑）→ 讀檔 + 讓 binding base64 編碼（:bytes 交原始位元組）。``
  (def low (string/ascii-lower spec))
  (cond
    (or (string/has-prefix? "data:" spec)
        (string/has-prefix? "http://" low)
        (string/has-prefix? "https://" low))
    {:url spec} # URL 直通
    (string/has-suffix? ".json" low) # 預先算好的 media 描述子
    (let [body (internal/read-file-text spec)]
      (if (nil? body)
        (do (eprintf "讀不到檔案：%s（--image/--media 描述子）" spec) nil)
        (let [[ok desc] (protect (json/decode body))]
          (cond
            (not ok) (do (eprintf "media 描述子 JSON 解析失敗（%s）" spec) nil)
            (and (dictionary? desc) (get desc "url")) {:url (get desc "url")} # 直通 url
            (and (dictionary? desc) (get desc "mime") (not (nil? (get desc "data"))))
            (let [[dok raw] (protect (base64/decode (get desc "data")))]
              (if (not dok)
                (do (eprintf "media 描述子的 data 不是合法 base64（%s）" spec) nil)
                {:url "" :mime (get desc "mime") :bytes raw})) # decode→bytes，不重讀原圖
            (do (eprintf "media 描述子形狀不符——需 {\"url\":…} 或 {\"mime\":…,\"data\":…}（%s）" spec) nil)))))
    # 二進位圖檔：讀檔 + binding base64
    (let [raw (internal/read-file-bytes spec)]
      (if (nil? raw)
        (do (eprintf "讀不到檔案：%s（--image/--media）" spec) nil)
        {:url "" :mime (mime-of spec) :bytes raw}))))
