# project.janet — 讓 janet-lsp 認得這是個 janet 專案（提供補全／跳轉的根）。
# 這裡不 build 任何東西：play.janet 用的是「裝好的」cllm 模組 llm.so ＋ spork，
# 都靠 env.sh 設好的 JANET_PATH 找到。
# ⚠ 編輯器要「從 source 過 ~/dev/cllm/env.sh 的 shell」開（janet-lsp 靠 JANET_PATH 找 (import llm)）。
(declare-project
  :name "cllm-lab-janet"
  :description "cllm Janet binding 遊樂場：改 play.janet 就是開發")
