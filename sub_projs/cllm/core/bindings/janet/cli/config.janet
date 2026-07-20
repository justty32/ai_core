# config.janet — 三層 config 來源解析（對齊 core/src/cli_config.cpp 的 load_into）。
#
# 來源優先序（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。本模組只處理「config 檔」這層
# （前二層）；命令列旗標覆寫在 main.janet。config 檔路徑：--config ＞ 環境變數 ＞
# ~/.config/llm/config.json。未列鍵靜默忽略（lenient；與 C++ glaze 嚴格解析刻意分歧）。

(import spork/json)
(import ./internal :as internal)
(import ./flags :as flags)

(defn default-config-path []
  ``~/.config/llm/config.json（對齊 cli_config.cpp：靠 HOME）；無 HOME 回 nil。``
  (def home (os/getenv "HOME"))
  (when home (string home "/.config/llm/config.json")))

(defn load-config [client has-config config-path]
  ``三層 config 來源前二層（對齊 cli_config.cpp 的 load_into）。把認得的鍵寫進 client
  （table，鍵＝ask 關鍵字）。回退出碼。``
  (var named false)
  (def cfg-path
    (cond
      has-config (do (set named true) config-path)
      (os/getenv internal/config-env-var) (do (set named true) (os/getenv internal/config-env-var))
      (default-config-path)))
  (if (nil? cfg-path)
    internal/exit-ok
    (do
      (def body (internal/read-file-text cfg-path))
      (cond
        (nil? body)
        (if named # 明指卻讀不到＝用法錯（點名是誰指的路）
          (do
            (def who (if has-config "--config" internal/config-env-var))
            (eprintf "讀不到檔案：%s（%s 指定的 config 檔）" cfg-path who)
            internal/exit-usage)
          internal/exit-ok) # 探測路徑讀不到＝沒設定檔，靜默用預設
        # 解析 JSON；壞 JSON＝用法錯
        (let [[ok data] (protect (json/decode body))]
          (if (not ok)
            (do (eprintf "config JSON 解析失敗（%s）" cfg-path) internal/exit-usage)
            (do
              (when (dictionary? data)
                (eachp [k v] data
                  (def kw (get flags/config-key->kw k))
                  (when kw (put client kw v))))
              internal/exit-ok)))))))
