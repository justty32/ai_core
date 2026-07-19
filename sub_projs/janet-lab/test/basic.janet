(use ../janet-lab/init)

# hello
(assert (= (hello) "Hello, world!") "hello 預設值")
(assert (= (hello "Janet") "Hello, Janet!") "hello 帶參數")

# 陣列
(assert (deep= (greetings @["a" "b"]) @["Hello, a!" "Hello, b!"]) "greetings 回陣列")

# 雜湊表
(def s (summarize @["Alice" "Bob"]))
(assert (= (s :count) 2) "summarize count")
(assert (deep= (s :names) @["Alice" "Bob"]) "summarize names")
(assert (= (get-in (summarize @["x"] true) [:names 0]) "X") "summarize upper?")

# JSON round-trip（原生 spork/json）
(def back (parse-json (summary-json @["Zed"])))
(assert (= (back :count) 1) "JSON round-trip count")
(assert (= (get-in back [:names 0]) "Zed") "JSON round-trip names")

(print "所有測試通過 ✓")
