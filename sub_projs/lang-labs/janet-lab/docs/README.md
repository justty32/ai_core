# Janet 教學 · 目錄

給這台機器上的 Janet 開發環境。每篇的程式碼都在 Janet 1.41.2 上實際跑過。

| # | 篇 | 重點 |
|---|----|------|
| 00 | [環境與工具鏈](00-環境與工具鏈.md) | 裝在哪、三個常用指令、原始碼編譯的理由 |
| 01 | [語言速成](01-語言速成.md) | 括號家族 `() [] {} @`、def/let、函式、條件、迴圈、`print` vs `pp` |
| 02 | [資料結構](02-資料結構.md) | array / tuple / table / struct，`@` 的意義，`get-in` |
| 03 | [JSON](03-json.md) | `spork/json`：字面≈JSON、encode/decode、null 陷阱、巢狀改值 |
| 04 | [CLI 參數](04-cli-argparse.md) | `spork/argparse` 四種 kind、自動 help、**子命令**（git 風格） |
| 05 | [jpm 與專案](05-jpm-與專案.md) | 專案結構、test / build / deps、裝套件 |
| 06 | [編輯器與 REPL](06-編輯器與-REPL.md) | Conjure `,ee` 工作流、parinfer、（可選）janet-lsp |
| 07 | [REPL 用法](07-repl.md) | 開關、`doc`、載入模組、`dyn`、跟 Conjure 的關係 |
| 08 | [巨集 macro](08-巨集-macro.md) | `~ , ,;`、`defmacro`、`macex1` 除錯、`with-syms` 衛生 |
| 09 | [Fiber 協程](09-fiber.md) | generator、例外即 fiber、信號遮罩、`ev` 非同步 |
| 10 | [與 C 互通](10-c-互通.md) | FFI（不編譯）/ native 模組 / 把 Janet 嵌進 C |
| 11 | [子行程 / 管線 / 信號](11-pipeline-signal.md) | `os/execute`、`sh/exec-slurp`、管線兩法、`proc-kill`、`sigaction` |

> 想「一頁看完」：專案根目錄有 **`janet-cheatsheet.html`**，把核心濃縮成單頁速查。
> 可跑範例在 [`examples/`](../examples/README.md)。

## 最快上手路線

1. `janet` 開 REPL（[07](07-repl.md)），把 [01](01-語言速成.md) 的片段貼進去玩。
2. 看 [02](02-資料結構.md) 建立 array / table 的手感。
3. 你最常用的兩個庫直接看 [03 JSON](03-json.md) 和 [04 argparse](04-cli-argparse.md)（含子命令）。
4. 進階招牌：[09 fiber](09-fiber.md)、[08 macro](08-巨集-macro.md)、[10 C 互通](10-c-互通.md)。
5. 看根目錄 `bin/main.janet` 與 [`examples/`](../examples/README.md)——都是可跑的實例。
