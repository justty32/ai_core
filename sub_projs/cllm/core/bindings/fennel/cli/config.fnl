;; config.fnl — 三層 config 來源解析（對齊 core/src/cli_config.cpp 的 load_into）。
;;
;; 來源優先序（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。本模組只處理「config 檔」這層；
;; 命令列旗標覆寫在 cli.fnl。config 檔路徑：--config ＞ 環境變數 ＞ ~/.config/llm/config.json。
;; 未列鍵靜默忽略（lenient；與 C++ glaze 的嚴格解析刻意分歧，見 core-py README 已知落差）。

(local json (require :dkjson))
(local flags (require :flags))
(local internal (require :internal))

;; config 檔允許的鍵（對齊 llm::abi::Client 欄位；未列鍵靜默忽略＝lenient）。
(local CONFIG_KEYS {})
(each [_ [_ field _] (ipairs flags.REFLECT_FLAGS)]
  (tset CONFIG_KEYS field true))

(fn default-config-path []
  "~/.config/llm/config.json（對齊 cli_config.cpp：靠 HOME）。"
  (let [home (os.getenv "HOME")]
    (if home (.. home "/.config/llm/config.json") nil)))

(fn load-config [client has-config config-path]
  "三層 config 來源前二層（對齊 cli_config.cpp 的 load_into）。就地灌入 client，回退出碼。"
  (var named false)
  (local cfg-path
    (if has-config (do (set named true) config-path)
        (os.getenv internal.CONFIG_ENV_VAR) (do (set named true) (os.getenv internal.CONFIG_ENV_VAR))
        (default-config-path)))
  (if (not cfg-path)
      internal.EXIT_OK
      (let [(body ferr) (internal.read-file-text cfg-path)]
        (if (not body)
            (if named ; 明指卻讀不到＝用法錯（點名是誰指的路）
                (let [who (if has-config "--config" internal.CONFIG_ENV_VAR)]
                  (io.stderr:write (.. "讀不到檔案：" cfg-path "（" who " 指定的 config 檔）\n"))
                  internal.EXIT_USAGE)
                internal.EXIT_OK) ; 探測路徑讀不到＝沒設定檔，靜默用預設
            (let [(data _ jerr) (json.decode body)]
              (if (not data)
                  (do (io.stderr:write (.. "config JSON 解析失敗（" cfg-path "）\n")) internal.EXIT_USAGE)
                  (do
                    (when (= (type data) "table")
                      (each [k v (pairs data)]
                        (when (. CONFIG_KEYS k) (tset client k v))))
                    internal.EXIT_OK)))))))

{: load-config}
