# internal.janet — CLI 共用的退出碼、環境變數鍵、檔案讀取小工具（對齊 core/src/cli_internal.hpp）。
#
# 葉模組：只用 janet 核心、不 import 套件內其他 cli 模組，作為其餘 CLI 模組
# （flags/config/media/output/argv/reqinput/main）的共用底，把依賴收成一張無環 DAG。

# 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。
(def exit-ok 0)
(def exit-usage 1)
(def exit-request 2)
(def exit-cancel 130)

(def config-env-var "LLM_CLI_CONFIG") # 對齊 cli_internal.hpp 的 kConfigEnvVar

(defn read-file-bytes [path]
  ``整檔讀成 raw bytes（媒體圖檔等），回 buffer；讀不到回 nil，由呼叫端轉退出碼。``
  (def f (file/open path :rb))
  (when f
    (def d (file/read f :all))
    (file/close f)
    (or d @"")))

(defn read-file-text [path]
  ``整檔讀成文字（config／media 描述子等），回 string；讀不到回 nil。``
  (def d (read-file-bytes path))
  (when d (string d)))
