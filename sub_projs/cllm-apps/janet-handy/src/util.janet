# util.janet —— janet-handy 四工具共用膠水碼
#   （shquote / script-dir 解 symlink / run-shell 透傳 exit / capture-shell /
#    檔案 I/O / stdin 非-tty 守衛 / list-stems / env-stem / utf8-head / trim）。
#
# 各工具（llme/zhtw/wf/mail）在頂層以
#   (dofile (string <script-dir> "/src/util.janet") :env (curenv))
# 把本檔的所有 def 併進呼叫端環境（見各工具開頭）。這樣即使工具被 symlink 進
# PATH，也靠 (os/realpath (get (dyn :args) 0)) 解到真實所在目錄再載入共用碼。
#
# 只用 Janet 內建：os/getenv、os/realpath、os/execute、os/spawn、slurp/spit、os/dir。
# 不 link libcllm、不用 cllm binding——真正的「手」仍轉呼外部命令（llm / claude / sibling）。
#
# 執行模型：與 Fennel 原型一致，組出「單一 shell 命令字串」後交給 /bin/sh -c 跑
#   （run-shell / capture-shell）；shquote 因此是真・load-bearing（吃中文/空白/旗標/管線）。
#   UTF-8：Janet 字串是位元組串，argv/檔案/輸出/信件內容全程原樣保留位元組，不亂碼。

# Lua %s 的空白集合：space \t \n \v \f \r（對齊 Fennel trim / slugify 的 %s）
(def ws "\x20\x09\x0a\x0b\x0c\x0d")

(defn getenv-nonempty
  "環境變數；未設或空字串回 nil。"
  [k]
  (def v (os/getenv k))
  (if (and v (not= v "")) v nil))

(defn trim
  "去頭尾空白（對齊 Fennel trim ^%s*(.-)%s*$）。"
  [s]
  (string/trim s ws))

# 單引號包住整串，內部 ' → '\''（吃中文/空白/旗標）。照 Fennel shquote 一字不差。
(defn shquote [s]
  (string "'" (string/replace-all "'" "'\\''" (string s)) "'"))

# ── script-dir：解 symlink 取「本執行檔所在目錄」（無尾斜線），供 (a) 找同層 sibling
#    工具 (b) 找 configs/ / inbox/。os/realpath 會把 PATH 上的 symlink 解回真實檔案。
(defn dirname [path]
  (def idx (last (string/find-all "/" path)))
  (if idx (string/slice path 0 idx) "."))

(defn script-path []
  (os/realpath (get (dyn :args) 0)))

(defn script-dir []
  (dirname (script-path)))

(defn file-exists? [p]
  (not (nil? (os/stat p))))

(defn read-file
  "讀整個檔（UTF-8 位元組原樣）；不存在回 nil。"
  [p]
  (if (file-exists? p) (string (slurp p)) nil))

(defn write-file [p body]
  (spit p body))

# ── stdin：只有非 tty（被 pipe/重導）才該讀。偵測用 `test -t 0`（不讀位元組），退出碼 0＝tty。
(defn stdin-piped? []
  (not (zero? (os/execute ["/bin/sh" "-c" "test -t 0"] :p))))

(defn read-stdin
  "一次讀 stdin 到 EOF（回字串）。"
  []
  (string (file/read stdin :all)))

# ── run-shell：把命令字串交給 /bin/sh -c 跑，繼承 stdout/stderr，回子命令 exit code。
#    對齊 Fennel 的 (os.execute cmd)。
(defn run-shell [cmd]
  (os/execute ["/bin/sh" "-c" cmd] :p))

# ── capture-shell：跑命令字串並捕捉 stdout 成字串（給 wf 的 LLM 分類用）。
(defn capture-shell [cmd]
  (def proc (os/spawn ["/bin/sh" "-c" cmd] :p {:out :pipe}))
  (def out (:read (proc :out) :all))
  (os/proc-wait proc)
  (if out (string out) ""))

# 掃某目錄頂層的 <name>.<ext>（不遞迴）。回已排序 stem 清單（對齊 ls -1 的字典序）。
# skip-underscore＝true 時排除 `_` 開頭（模板/隱藏，llme 不列入可用 endpoint）。
(defn list-stems [dir ext &opt skip-underscore]
  (def suffix (string "." ext))
  (def entries (try (os/dir dir) ([_] @[])))
  (def out @[])
  (each name entries
    (when (string/has-suffix? suffix name)
      (def stem (string/slice name 0 (- (length name) (length suffix))))
      (when (and (> (length stem) 0)
                 (not (and skip-underscore (= (get stem 0) (chr "_")))))
        (array/push out stem))))
  (sort out)
  out)

# endpoint 名 → env 慣例的大寫識別子（先大寫，非英數→_，如 deep-seek → DEEP_SEEK）。
# 對齊 Fennel (: (s:upper) :gsub "[^%w]" "_")。
(defn env-stem [s]
  (def up (string/ascii-upper s))
  (def b (buffer))
  (each c up
    (if (or (and (>= c (chr "0")) (<= c (chr "9")))
            (and (>= c (chr "A")) (<= c (chr "Z"))))
      (buffer/push-byte b c)
      (buffer/push-byte b (chr "_"))))
  (string b))

# 取前 n 個「字元」（UTF-8 安全，不切半個字）。走位元組、只在字元起始位元組計數。
(defn utf8-head [s n]
  (def len (length s))
  (var chars 0)
  (var i 0)
  (var cut len)
  (while (< i len)
    (def by (get s i))
    (when (not= (band by 0xC0) 0x80)   # 非 continuation byte ＝ 一個字元的起始
      (if (= chars n) (do (set cut i) (break)))
      (++ chars))
    (++ i))
  (string/slice s 0 cut))
