;; media.fnl — --image/--media 的 MIME 對照與三分流取值（＋純 Lua base64 解碼）。
;;
;; mime-of／ext-of 對齊 cli_config.cpp 的同名對照表。build-media-item 是 core-py 特有的取值分流：
;; 讓 --media 除了讀二進位圖檔，也能吃「已編碼」形式（data:/http URL、或 .json 描述子），省掉每次
;; 重算 base64。內核期待 media 的 :bytes 是 raw 位元（由 core 自行 base64 編碼），故描述子的
;; base64 需先解回 raw。零額外相依：base64 解碼用純 Lua 手刻（見 b64decode）。

(local json (require :dkjson))
(local internal (require :internal))

(fn mime-of [path]
  "副檔名 → MIME（對齊 cli_config.cpp 的 mime_of）。"
  (let [ext (-> (or (path:match "%.([%w]+)$") "") (: :lower))]
    (or (. {:png "image/png" :jpg "image/jpeg" :jpeg "image/jpeg"
            :gif "image/gif" :webp "image/webp" :wav "audio/wav" :mp3 "audio/mpeg"} ext)
        "application/octet-stream")))

(fn ext-of [mime]
  "MIME → 落檔副檔名（對齊 cli_config.cpp 的 ext_of）。"
  (or (. {"image/png" "png" "image/jpeg" "jpg" "image/gif" "gif"
          "image/webp" "webp" "audio/wav" "wav" "audio/mpeg" "mp3"} mime) "bin"))

(local B64 "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/")
(fn b64decode [data]
  "純 Lua base64 解碼 → raw 位元字串（供描述子 {mime,data} 直通內核）。"
  (let [clean (-> data (: :gsub "[^%w%+/=]" "") (: :gsub "=" ""))]
    (var bits "")
    (each [c (clean:gmatch ".")]
      (let [idx (- (B64:find c 1 true) 1)]
        (for [i 5 0 -1]
          (set bits (.. bits (if (> (% (// idx (^ 2 i)) 2) 0) "1" "0"))))))
    (var out "")
    (for [i 1 (- (length bits) 7) 8]
      (let [byte (bits:sub i (+ i 7))]
        (var v 0)
        (for [j 1 8] (set v (+ (* v 2) (if (= (byte:sub j j) "1") 1 0))))
        (set out (.. out (string.char v)))))
    out))

(fn err-nil [msg]
  "印診斷到 stderr 後回 nil（取值失敗的共同出口）。"
  (io.stderr:write (.. msg "\n"))
  nil)

(fn build-desc [spec]
  "spec＝.json 描述子路徑 → media item（{url} 直通或 {mime,data} 解碼直通）。失敗回 nil。"
  (let [(body ferr) (internal.read-file-text spec)]
    (if (not body) (err-nil (.. "讀不到檔案：" spec "（--image/--media 描述子）"))
        (let [(desc _ jerr) (json.decode body)]
          (if (not desc) (err-nil (.. "media 描述子 JSON 解析失敗（" spec "）"))
              (and (= (type desc) "table") desc.url) {:url desc.url}
              (and (= (type desc) "table") desc.mime (not= desc.data nil))
              {:url "" :mime desc.mime :bytes (b64decode desc.data)}
              (err-nil (.. "media 描述子形狀不符——需 {\"url\":…} 或 {\"mime\":…,\"data\":…}（" spec "）")))))))

(fn build-media-item [spec]
  "--image/--media 一個值 → 內核 media item（三分流）。成功回 table、失敗印 stderr 回 nil。

  1. data:／http(s):// URL → 直接當 :url 送、不編碼。
  2. .json 副檔名（用副檔名判定）→ 讀該檔當「預先算好的描述子」直通（見 build-desc）。
  3. 其餘（二進位圖檔路徑）→ 讀檔 + 交內核 base64（現行行為，不變）。"
  (let [low (spec:lower)]
    (if (or (= (spec:sub 1 5) "data:") (= (low:sub 1 7) "http://") (= (low:sub 1 8) "https://"))
        {:url spec}
        (= (low:sub -5) ".json")
        (build-desc spec)
        (let [(raw ferr) (internal.read-file-bytes spec)]
          (if (not raw) (err-nil (.. "讀不到檔案：" spec "（--image/--media）"))
              {:url "" :mime (mime-of spec) :bytes raw})))))

{: mime-of : ext-of : build-media-item}
