# cl-lab

Common Lisp（SBCL）開發環境試驗場 —— 一個能跑的 ASDF 專案 + 一整套中文教學。

SBCL 2.6.6 + Quicklisp（系統既有），已裝好 **jzon**（JSON）、**clingon**（CLI，含子命令）、
**cffi**（C FFI）、**fiveam**（測試）、**swank**（給 nvim 連）。環境細節見
[docs/00-環境與工具鏈.md](docs/00-環境與工具鏈.md)。

## 馬上試

```sh
scripts/run.sh run -n Alice -n Bob 二次元    # 打招呼（純文字）
scripts/run.sh run -n alice --upper --json   # JSON 輸出
scripts/run.sh run --help                    # clingon 自動生成的說明
scripts/run.sh test                          # 跑測試（fiveam，8 檢查）
scripts/run.sh build                          # 編獨立執行檔 → build/cl-lab
scripts/run.sh repl                          # 開已載入 cl-lab 的 REPL
```

`src/main.lisp` 一支就示範了你最常用的：**CLI 參數（clingon）、JSON（jzon）、串列 / 雜湊表**。

## 教學

- 分篇 markdown：[docs/](docs/README.md)（00 環境 → 09 C FFI，逐篇遞進）
- 單頁速查：**`cl-cheatsheet.html`**（把語言 + 庫濃縮成一頁，開瀏覽器即看）
- 可跑範例：[examples/](examples/README.md)（ffi / subcommands / conditions / macros）

CL 的兩塊靈魂特別看：[條件系統](docs/08-conditions.md)（restart，比 try/catch 多一維）、
[macro](docs/07-macro.md)（Lisp 宏系統的祖宗）。

## 編輯器

nvim 已配 Conjure（走 Swank，connect 模式）+ parinfer + 彩虹括號。
先 `scripts/run.sh swank`，再在 nvim `.lisp` 檔按 `,cc` 連、`,ee` 求值。見
[docs/06-編輯器與-repl.md](docs/06-編輯器與-repl.md)。

## 結構

```
cl-lab.asd            ASDF 系統定義（依賴、build 執行檔設定）
src/package.lisp      套件（命名空間）
src/main.lisp         核心 + clingon CLI
tests/main.lisp       fiveam 測試
scripts/run.sh        run / test / build / repl / swank
examples/             可跑範例
docs/                 分篇教學
cl-cheatsheet.html    單頁速查表
```

> 註：`~/quicklisp/local-projects/cl-lab` 有一個指向本目錄的符號連結，讓 `(ql:quickload :cl-lab)`
> 在任何地方都找得到這個專案。
