# 05 · jpm 與專案結構

`jpm`（Janet Project Manager）管 build、測試、依賴、安裝。

## 這個專案長怎樣

```
janet-lab/
├─ project.janet        ← 專案宣告：名字、依賴、要編的東西
├─ janet-lab/
│  └─ init.janet        ← 核心模組（純函式），別人 import 這個
├─ bin/
│  └─ main.janet        ← 執行檔進入點（argparse CLI）
├─ test/
│  └─ basic.janet       ← 測試（jpm test 會跑）
└─ docs/                ← 你正在看的教學
```

## project.janet 三個宣告

```janet
(declare-project
  :name "janet-lab"
  :version "0.1.0"
  :dependencies ["spork"])        # 依賴清單，jpm deps 會裝

(declare-source                   # 宣告可被 import 的原始碼
  :prefix "janet-lab"
  :source ["janet-lab/init.janet"])

(declare-executable               # 宣告要編成單一執行檔的進入點
  :name "janet-lab"
  :entry "bin/main.janet"
  :install false)
```

## 日常指令

```sh
jpm deps          # 依 project.janet 裝 / 更新依賴
jpm test          # 跑 test/ 底下所有 .janet
jpm build         # 依 declare-executable 編出 build/janet-lab（單一原生執行檔）
./build/janet-lab # 跑編好的執行檔
jpm clean         # 清掉 build/
```

`jpm build` 出來的是**獨立原生執行檔**（把 janet runtime 打包進去），可以直接複製到別台跑，
不需要對方裝 janet。

## 開發時通常不用每次 build

改 → 跑 最快的循環是直接跑腳本，不必 build：

```sh
janet bin/main.janet -n test      # 直接跑，秒回
jpm test                          # 改完跑測試
```

`build` 只在「要交付一個執行檔」時才需要。

## 裝別人的套件

```sh
jpm install spork                              # 從官方 pkg 清單裝
jpm install https://github.com/user/repo.git   # 直接從 git URL 裝
```

裝到 `~/.local/lib/janet/`，任何專案都能 `(import 套件名)`。這台機器的
`spork`、`janet-lsp` 就是這樣裝的。

## 加一個依賴

1. 在 `project.janet` 的 `:dependencies` 加上套件名。
2. `jpm deps` 裝下來。
3. 程式裡 `(import 套件名)`。

下一步：[06-編輯器與-REPL.md](06-編輯器與-REPL.md)。
