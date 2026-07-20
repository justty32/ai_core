(declare-project
  :name "janet-try-1"
  :description "用 Janet 打 Anthropic API 的最小 cllm 應用（經 anthropic-proxy sidecar）"
  :version "0.1.0"
  # spork 提供 argparse/json；llm（cllm 的 janet binding）由平行 agent 裝到 JANET_PATH，
  # 不列在這裡（它不是 jpm 套件，靠 ~/dev/cllm/env.sh 掛上路徑後即可 (import llm)）。
  :dependencies ["spork"])

(declare-source
  :prefix "janet-try-1"
  :source ["janet-try-1/init.janet"])

# 產生單一原生執行檔 build/janet-try-1（jpm build）
(declare-executable
  :name "janet-try-1"
  :entry "bin/main.janet"
  :install false)
