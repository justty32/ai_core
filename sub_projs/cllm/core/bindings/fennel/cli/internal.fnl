;; internal.fnl — CLI 共用的退出碼、環境變數鍵、檔案讀取小工具（對齊 core/src/cli_internal.hpp）。
;;
;; 葉模組：只依 Lua 標準庫、不 require 套件內其他模組，作為其餘 CLI 模組（flags/config/media/
;; output/argv/reqinput/cli）的共用底，把依賴收成一張無環 DAG。
;;
;; ⚠ 呼叫的是 lua 的 llm binding（跑在 lua 5.4）：table 鍵一律底線，本檔亦然。

;; 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。
(local EXIT_OK 0)
(local EXIT_USAGE 1)
(local EXIT_REQUEST 2)
(local EXIT_CANCEL 130)

(local CONFIG_ENV_VAR "LLM_CLI_CONFIG") ; 對齊 cli_internal.hpp 的 kConfigEnvVar

(fn read-all [path mode]
  "整檔讀出，回 (內容 nil)；讀不到回 (nil 錯訊)——由呼叫端轉退出碼／印 stderr。"
  (let [(f err) (io.open path mode)]
    (if f
        (let [data (f:read "*a")]
          (f:close)
          (values (or data "") nil))
        (values nil (or err (.. "讀不到檔案：" path))))))

(fn read-file-bytes [path]
  "整檔讀成 raw bytes（媒體圖檔等）。Lua 字串本就位元安全。"
  (read-all path :rb))

(fn read-file-text [path]
  "整檔讀成文字（config／media 描述子等）；POSIX 下與 read-file-bytes 等價。"
  (read-all path :r))

{: EXIT_OK : EXIT_USAGE : EXIT_REQUEST : EXIT_CANCEL : CONFIG_ENV_VAR
 : read-file-bytes : read-file-text}
