# janet-lab 的核心模組 —— 純函式放這裡，bin/main.janet 與 test/ 都 import 它。
# 這支檔案刻意把你最常用的四樣東西各示範一次：JSON、陣列、雜湊表、（CLI 在 bin/main.janet）。
(import spork/json)

(defn hello
  "回傳打招呼字串。who 省略時預設 world。"
  [&opt who]
  (default who "world")
  (string "Hello, " who "!"))

# ── 陣列 (array, @[...]) ─────────────────────────────────────────────
# @[...] 是可變陣列；[...] 是不可變 tuple。core 內建，不需任何庫。
(defn greetings
  "把一串名字 map 成招呼陣列。回傳可變陣列 @[...]。"
  [names]
  (map hello names))

# ── 雜湊表 (hash map, @{...}) ────────────────────────────────────────
# @{...} 是可變 table；{...} 是不可變 struct。put/get/update 都作用在 table 上。
(defn summarize
  "把名字陣列整理成一張摘要雜湊表。"
  [names &opt upper?]
  (def norm (if upper? (map string/ascii-upper names) names))
  @{:count   (length norm)
    :names   norm
    :hellos  (greetings norm)})

# ── JSON ────────────────────────────────────────────────────────────
# spork/json 是原生 (C) 模組——它能載入就證明整條原生編譯鏈是通的。
(defn summary-json
  "把摘要編成 JSON 字串（indent 2 好讀）。"
  [names &opt upper?]
  (json/encode (summarize names upper?) "  " "\n"))

(defn parse-json
  "反向：把 JSON 字串解回 Janet 資料結構。"
  [s]
  (json/decode s true))   # true = 用 keyword 當 key
