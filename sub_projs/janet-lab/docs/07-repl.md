# 07 · REPL 用法

REPL 是 Janet 開發的核心。「開著它、邊寫邊丟進去試」比反覆存檔重跑快得多。

## 開 / 關

```sh
janet                 # 開 REPL
```

```janet
repl:1:> (+ 1 2)
3
repl:2:> (quit)       # 離開（或按 Ctrl-D）
```

提示字元 `repl:2:>` 的數字是輸入序號。多行運算式沒打完會自動等你補完括號。

## 內建的救命指令

| 指令 | 作用 |
|------|------|
| `(doc name)` | 看某個東西的 docstring + 型別 + 定義位置 |
| `(doc)` | 列出所有可用綁定（很長） |
| `(pp x)` | pretty-print 一個值（看 array / table 內容） |
| `(type x)` | 看型別 |
| `(quit)` / Ctrl-D | 離開 |

```janet
repl:1:> (doc map)          # 查 map 怎麼用
repl:2:> (doc json/encode)  # 查庫函式（要先 import）
```

`(doc name)` 是你在 REPL 裡最常按的——不必離開去翻文件。

## 載入東西進 REPL

```janet
(import spork/json)                  # 載入一個模組
(import ./janet-lab/init :as lab)    # 載入本專案的檔（相對路徑，以 REPL 的 cwd 為準）
(dofile "some.janet")                # 直接執行一支檔
```

小提醒：`import` 相對路徑是相對「你啟動 janet 的目錄」。所以進專案要 `cd` 到專案根目錄再開
`janet`，`(import ./janet-lab/init)` 這種才對得上。

## 動態變數 dyn

REPL / 腳本共用一組動態變數（`(dyn :key)` 讀、`(setdyn :key v)` 寫）。最常見的是命令列參數：

```janet
(dyn :args)          # 這次執行的命令列參數陣列
(dyn :executable)    # janet 執行檔路徑
```

## 用 nvim 的 Conjure 取代手打 REPL

實務上你不會一直手貼進 REPL——在 nvim 裡用 Conjure，游標放在 form 上按 `,ee` 就送進背景
REPL 求值、結果顯示在 log。等於「REPL 一直開、但你人在編輯器裡」。完整鍵位見
[06-編輯器與-REPL.md](06-編輯器與-REPL.md)。

兩者關係：Conjure 背後就是起一個 `janet -n -s` 的 stdio REPL，把你的 form 丟進去。所以你在
REPL 學到的一切（`doc`、`import`、`pp`）在 Conjure 裡照用。

下一步：[08-巨集-macro.md](08-巨集-macro.md)。
