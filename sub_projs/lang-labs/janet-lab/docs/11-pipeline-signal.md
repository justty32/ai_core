# 11 · 子行程、管線、信號

Janet 內建一整套 POSIX 行程控制（`os/*`），加上 spork/sh 的便利函式。完整可跑範例：
[`examples/pipeline.janet`](../examples/pipeline.janet)。

## 跑指令 + 看結果

```janet
(os/execute ["echo" "hi"] :p)     # => 0   :p = 用 PATH 找執行檔，回傳 exit code
(os/execute ["false"] :px)        # :x = 非零 exit 直接丟 error
```

旗標（可疊）：`:p` 用 PATH、`:x` 非零就報錯、`:e` 傳環境變數表。

## 捕捉輸出

最省事——spork/sh 的 `exec-slurp`（像 Python 的 `check_output`）：

```janet
(import spork/sh)
(string/trim (sh/exec-slurp "git" "rev-parse" "HEAD"))   # => 當前 commit hash
```

要更細的控制就用 `os/spawn` + `:pipe`：

```janet
(def p (os/spawn ["echo" "hello"] :p {:out :pipe}))
(def out (:read (p :out) :all))   # 從子行程 stdout 讀全部
(os/proc-wait p)                  # 等它結束（別忘，否則變殭屍）
```

`os/spawn` 第三參是重導表：`{:in X :out Y :err Z}`，每個值可以是 `:pipe`（做成管線）、
一個檔案 stream、或 `nil`（繼承父行程）。

## 管線 a | b

### 法一：Janet 自己當管線一端（推薦，最好控制）

把資料從 Janet 寫進子行程 stdin、再讀 stdout：

```janet
(def p (os/spawn ["sort"] :p {:in :pipe :out :pipe}))
(:write (p :in) "banana\napple\ncherry\n")
(:close (p :in))                  # ★ 關 stdin → 子行程收到 EOF 才動
(def sorted (:read (p :out) :all))
(os/proc-wait p)                  # => "apple\nbanana\ncherry"
```

### 法二：多段 `a | b | c` 交給 shell

要一長串管線，最直接是丟給 `sh -c`：

```janet
(os/execute ["sh" "-c" "printf 'b\\na\\n' | sort | tr a-z A-Z"] :p)   # => A / B
```

> 為什麼不直接把 `proc1` 的 `:out` 接到 `proc2` 的 `:in`？因為 Janet 的 `:pipe` 串流是
> **非阻塞** fd，另一個子行程去讀會撞 `EAGAIN`（Resource temporarily unavailable）。
> 要嘛用上面法一由 Janet 中轉，要嘛交給 shell。踩過，記著。

## 送信號給子行程

```janet
(def child (os/spawn ["sleep" "30"] :p))
(os/proc-kill child)              # 預設送 SIGKILL
(os/proc-kill child false :term)  # (proc wait? signal)：送 SIGTERM
(os/proc-wait child)              # => 143  (= 128 + 15 SIGTERM)
```

合法 signal keyword：`:term :int :hup :kill :usr1 :usr2 :quit`（`:winch` 這種不支援）。

## 處理自己收到的信號

```janet
(os/sigaction :int (fn [] (print "收到 SIGINT，優雅收尾…") (os/exit 0)))
```

`(os/sigaction which handler)`——`which` 是 signal keyword，`handler` 是無參函式。

⚠ **關鍵**：handler 是掛在 **ev 事件迴圈**上被觸發的。所以程式要**有 ev loop 在跑**（例如你用
`ev/` 做非同步、或明確 `(ev/sleep n)` / 進 `ev/` 服務迴圈）handler 才會實際執行。純同步、跑完就
結束的腳本註冊了也不會觸發。長駐服務（server / daemon）才是它的舞台。傳 `nil` 當 handler 可
還原預設行為。

## 小抄

| 想做 | 怎麼寫 |
|------|--------|
| 跑指令看 exit | `(os/execute ["cmd" ...] :p)` |
| 跑指令、非零就報錯 | `(os/execute [...] :px)` |
| 跑指令抓輸出 | `(sh/exec-slurp "cmd" ...)` |
| 餵 stdin / 讀 stdout | `os/spawn` + `{:in :pipe :out :pipe}` + `:write`/`:read` |
| 多段管線 | `(os/execute ["sh" "-c" "a | b | c"] :p)` |
| 殺子行程 | `(os/proc-kill p false :term)` |
| 等子行程 | `(os/proc-wait p)` |
| 接自己的信號 | `(os/sigaction :int handler)`（需 ev loop） |

回目錄：[docs/README.md](README.md)。
