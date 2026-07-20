# Common Lisp 教學 · 目錄

給這台機器上的 SBCL 開發環境。每篇的程式碼都在 SBCL 2.6.6 上實際跑過。

| # | 篇 | 重點 |
|---|----|------|
| 00 | [環境與工具鏈](00-環境與工具鏈.md) | SBCL / Quicklisp / ASDF、`scripts/run.sh` |
| 01 | [語言速成](01-語言速成.md) | `defun`/`let`/`lambda`、條件、`loop`、`format` |
| 02 | [資料結構](02-資料結構.md) | list / vector / hash-table，何時用哪個 |
| 03 | [JSON](03-json.md) | `com.inuoe.jzon` stringify / parse / 巢狀 |
| 04 | [CLI 參數](04-cli-clingon.md) | `clingon`：選項、**原生子命令**（git 風格） |
| 05 | [ASDF 與 Quicklisp](05-asdf-quicklisp.md) | 系統定義、依賴、build 執行檔、裝庫 |
| 06 | [編輯器與 REPL](06-編輯器與-repl.md) | Conjure + Swank `,cc`/`,ee`、互動開發心法 |
| 07 | [巨集 macro](07-macro.md) | `` ` ``/`,`/`,@`、`gensym`、`macroexpand-1` |
| 08 | [條件系統](08-conditions.md) | conditions / restarts —— CL 最強的一塊 |
| 09 | [C FFI](09-c-ffi.md) | `cffi`：`defcfun` 直接呼叫共享庫 |

> 想「一頁看完」：專案根目錄有 **`cl-cheatsheet.html`**，把核心濃縮成單頁速查。
> 可跑範例在 [`examples/`](../examples/README.md)。

## 最快上手路線

1. `scripts/run.sh repl` 開一個已載入 cl-lab 的 REPL，把 [01](01-語言速成.md) 的片段貼進去玩。
2. 看 [02](02-資料結構.md) 建立 list / vector / hash-table 的手感。
3. 你最常用的兩個庫直接看 [03 JSON](03-json.md) 和 [04 clingon](04-cli-clingon.md)（含子命令）。
4. CL 的靈魂：[08 條件系統](08-conditions.md)、[07 macro](07-macro.md)。
5. 看 `src/main.lisp` 與 [`examples/`](../examples/README.md)——都是可跑的實例。
