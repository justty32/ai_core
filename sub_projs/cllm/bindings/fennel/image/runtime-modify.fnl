;; runtime-modify.fnl — Fennel/Lua 執行期修改（活的，但無映像持久化）。
;; 跑：source ~/dev/env.sh 後  fennel runtime-modify.fnl
(global greet (fn [n] (.. "v1：嗨 " n)))
(fn caller [n] (greet n))
(print (.. "① 改前：" (caller "星野")))
(global greet (fn [n] (.. "v2：喲 " n "～（執行期改過）")))   ; 執行期換 global
(print (.. "① 改後：" (caller "星野")))                       ; caller 立即用新版
(print "② 互動玩法：fennel（不給檔）進 REPL，貼新 (fn …) 或 ,reload 熱更；Lua 函式是值、可隨時重綁。")
(print "   ⚠ Lua/Fennel 無記憶體映像 dump：改動存不成映像，固化＝重存原始碼再 AOT 編。")
