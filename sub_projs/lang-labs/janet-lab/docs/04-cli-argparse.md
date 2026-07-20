# 04 · CLI 參數（spork/argparse）

`spork/argparse` 是官方標準庫的命令列解析器。**自動生成 `--help` 與 usage**，不用自己排。

```janet
(import spork/argparse :as ap)
```

## 基本形狀

```janet
(def res
  (ap/argparse
    "程式一句話說明"
    "name"  {:kind :accumulate :short "n" :help "名字，可重複給"}
    "upper" {:kind :flag       :short "u" :help "轉大寫"}
    "level" {:kind :option      :short "l" :default "1" :help "等級"}
    :default {:kind :accumulate :help "位置參數（不帶 - 的）"}))
```

`argparse` 讀的是 `(dyn :args)`（真正的命令列）。解析成功回傳一張 table，用**選項名字串**取值；
解析失敗或 `--help` 時它會**自己印出 usage 並回傳 `nil`**——所以標準寫法是：

```janet
(unless res (os/exit 1))    # nil 代表失敗 / 印過 help，直接退出
```

## 四種 `:kind`

| kind | 意思 | 取出來的型別 | 例 |
|------|------|--------------|-----|
| `:flag` | 開關，有給就是 `true` | boolean | `--upper` → `true` |
| `:multi` | 開關可給多次，數次數 | integer | `-vvv` → `3` |
| `:option` | 帶一個值，最後一次為準 | string | `--level 5` → `"5"` |
| `:accumulate` | 帶值、可給多次、收成陣列 | array | `-n A -n B` → `@["A" "B"]` |

## 實際解析結果

給定參數 `-n Alice --name Bob --upper pos1 pos2`：

```janet
(res "name")     # => @["Alice" "Bob"]   累積成陣列
(res "upper")    # => true               flag
(res "level")    # => "1"                沒給 → 用 :default
(res :default)   # => @["pos1" "pos2"]   位置參數（特殊 key :default）
```

**兩個 `:default` 別搞混**：
- 選項規格裡的 `:default 值` = 這個選項沒給時的預設值。
- 特殊選項名 `:default {...}` = 收集所有不帶 `-`／`--` 的**位置參數**。

## 其它有用的 key

- `:required true`——沒給就報錯。
- `:map 函式`——取值後自動轉換，例如 `:map scan-number` 把字串轉數字。
- `:short-circuit true`——碰到就停止解析（`--help` 就是這樣做的），其餘參數放在結果的 `:rest`。

`:option` / `:accumulate` 取回來**都是字串**，要數字自己轉（`scan-number` 或 `:map scan-number`）。

## 這個專案的實例

`bin/main.janet` 就是一支完整可跑的 argparse 程式，合併了 `--name` 與位置參數、支援 `--upper`
`--json`。直接看它、或跑跑看：

```sh
janet bin/main.janet -n Alice -n Bob 二次元        # 純文字輸出
janet bin/main.janet -n alice --upper --json       # JSON 輸出
janet bin/main.janet --help                        # 自動生成的說明
```

## 子命令（git-submodule 風格）

spork/argparse **沒有**像 Python `add_subparsers()` 那種內建子命令機制，但用「dispatch + 每個
子命令各跑一次 argparse」就能做到，而且更直觀。關鍵是 argparse 的 `:args` 參數——可以餵一個
自訂的參數陣列（它會把 `[0]` 當程式名忽略）：

```janet
(import spork/argparse :as ap)

(defn cmd-add [args]
  (def res (ap/argparse "add：新增一個 submodule"
             :args ["tool add" ;args]                    # 墊一個程式名在 [0]
             "name"   {:kind :option :short "n"}
             "branch" {:kind :option :short "b" :default "main"}
             :default {:kind :accumulate :help "<url>"}))
  (unless res (os/exit 1))
  (printf "ADD url=%q name=%q branch=%q"
          (get-in res [:default 0]) (res "name") (res "branch")))

(defn cmd-list [args]
  (def res (ap/argparse "list：列出" :args ["tool list" ;args]
             "verbose" {:kind :flag :short "v"}))
  (unless res (os/exit 1))
  (printf "LIST verbose=%q" (res "verbose")))

(def subcommands {"add" cmd-add  "list" cmd-list})

(defn main [& argv]
  (def sub (get argv 1))                 # argv[0]=腳本名, argv[1]=子命令
  (def rest (array/slice argv 2))        # 其餘丟給子命令
  (if-let [handler (get subcommands sub)]
    (handler rest)
    (do (eprintf "未知子命令 %q，可用：%q" sub (keys subcommands))
        (os/exit 1))))
```

跑起來（完整可跑版在 [`examples/subcommands.janet`](../examples/subcommands.janet)）：

```sh
janet examples/subcommands.janet add https://x.git --name libs -b dev
#   ADD url="https://x.git" name="libs" branch="dev"
janet examples/subcommands.janet list -v          # LIST verbose=true
janet examples/subcommands.janet add --help       # 子命令自己的 usage（自動生成）
```

每個子命令**各自擁有 `--help`、各自的選項集**，正是你要的 `git submodule add ...` 那種感覺。
要再深一層（`git submodule add`）就是**遞迴**：`submodule` 的 handler 內部再對 `(array/slice
args ...)` 做一次同樣的 dispatch。

下一步：[05-jpm-與專案.md](05-jpm-與專案.md)。
