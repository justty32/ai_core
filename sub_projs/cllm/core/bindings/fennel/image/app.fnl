;; app.fnl — 要 AOT 編成獨立 .lua 的 Fennel 程式（用 cllm）。
;; 編：fennel --compile app.fnl > app.lua
;; 跑（免 fennel，只要 lua + llm.so）：lua5.4 app.lua "$CLLM_FIXTURES/"
(local llm (require :llm))
(local base (or (. arg 1) ""))
(print (llm.ask "你好" (.. base "fake/chat/completions")))
