#!/usr/bin/env janet
# 子行程 / 管線 / 信號 示範。
# 跑法：janet examples/pipeline.janet
# 詳解見 docs/11-pipeline-signal.md
(import spork/sh)

# 1) 最簡單：跑一個指令、看 exit code（:p = 用 PATH 找執行檔）
(prin "1) os/execute: ") (flush)
(printf "exit=%q" (os/execute ["echo" "hi"] :p))

# 2) 捕捉輸出最省事：spork/sh 的 exec-slurp
(printf "2) sh/exec-slurp => %q" (string/trim (sh/exec-slurp "echo" "captured")))

# 3) 餵資料進子行程、再讀出來（Janet 自己當管線的一端，最穩健）
(def p (os/spawn ["sort"] :p {:in :pipe :out :pipe}))
(:write (p :in) "banana\napple\ncherry\n")
(:close (p :in))                       # 關 stdin → sort 收到 EOF 才開始排
(def sorted (:read (p :out) :all))
(os/proc-wait p)
(printf "3) sort => %q" (string/trim sorted))

# 4) 真的要 a | b | c 多段管線：交給 shell 最直接
(prin "4) shell pipeline: ") (flush)
(os/execute ["sh" "-c" "printf 'b\\na\\n' | sort | tr a-z A-Z"] :p)

# 5) 送信號給子行程
(def child (os/spawn ["sleep" "30"] :p))
(os/proc-kill child false :term)       # 送 SIGTERM（signal 是 keyword）
(printf "5) 被 kill 的 sleep wait => %q" (os/proc-wait child))

# 6) 註冊自己的信號處理器（要有 ev 事件迴圈在跑，handler 才會真的被觸發）
(os/sigaction :int (fn [] (print "收到 SIGINT，優雅收尾…")))
(print "6) SIGINT handler 已註冊（本 demo 不進 ev loop，故不會實際觸發）")
