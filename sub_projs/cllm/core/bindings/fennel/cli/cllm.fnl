;; cllm.fnl — Fennel 版 llm CLI 進入點（對齊 core-py 的 __main__.py）。
;;
;; 用 fennel 跑：fennel cllm.fnl [旗標...] [prompt...]（或用同資料夾的薄殼 ./cllm，它先設好 LUA 路徑）。
;; 自我定位：把本檔所在資料夾加進 fennel.path，讓 (require :internal) 等姊妹模組不論 cwd 都解得到。
;; ⚠ SIGINT／取消（退出碼 130）：Lua 標準庫無信號處理，本版做不到 C++ 版的乾淨中止——已知落差。

(local fennel (require :fennel))
(local self (. arg 0))
(local here (or (self:match "(.*)[/\\][^/\\]*$") "."))
(set fennel.path (.. here "/?.fnl;" fennel.path))

(local cli (require :cli))
(os.exit (cli.main arg))
