#!/usr/bin/env janet
# 子命令 dispatcher（git-submodule 風格）。
# 跑法：
#   janet examples/subcommands.janet add https://x.git --name libs -b dev
#   janet examples/subcommands.janet list -v
#   janet examples/subcommands.janet add --help
# 詳解見 docs/04-cli-argparse.md 的「子命令」段。
(import spork/argparse :as ap)

# 每個子命令一個 handler，各自跑自己的 argparse。
# 關鍵：argparse 讀 :args 陣列，並把 [0] 當程式名忽略，所以前面墊一個名字。
(defn cmd-add [args]
  (def res (ap/argparse "add：新增一個 submodule"
             :args ["tool add" ;args]
             "name"   {:kind :option :short "n" :help "本地名稱"}
             "branch" {:kind :option :short "b" :default "main" :help "分支"}
             :default {:kind :accumulate :help "<url>"}))
  (unless res (os/exit 1))
  (printf "ADD url=%q name=%q branch=%q"
          (get-in res [:default 0]) (res "name") (res "branch")))

(defn cmd-list [args]
  (def res (ap/argparse "list：列出" :args ["tool list" ;args]
             "verbose" {:kind :flag :short "v"}))
  (unless res (os/exit 1))
  (printf "LIST verbose=%q" (res "verbose")))

(def subcommands
  {"add"  cmd-add
   "list" cmd-list})

(defn main [& argv]
  # argv[0]=腳本名, argv[1]=子命令, 其餘給子命令
  (def sub (get argv 1))
  (def rest (array/slice argv 2))
  (if-let [handler (get subcommands sub)]
    (handler rest)
    (do (eprintf "未知子命令 %q，可用：%q" sub (keys subcommands))
        (os/exit 1))))

# 巢狀（git submodule add）就是遞迴：頂層 dispatch 到 "submodule"，
# 那個 handler 再對 (array/slice args ...) 做一次同樣的 dispatch。
