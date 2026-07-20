# vscode-comfy — comfy 語法糖高亮（注入文法）

VSCode 高亮是**靜態 TextMate 文法**，跟 comfy 的 reader macro（執行期）是兩套系統，所以 comfy 的糖
預設不會被特別上色。這個小擴充用**注入文法**（injection grammar）補上，不改 Lisp 語言本身。

補的兩項（其餘 Alive 已處理）：
- `true` / `false` → `constant.language.boolean.comfy`（Alive 只認 `t`/`nil`，不認這兩個）
- `'a'`、`'\n'` 等字元字面量 → `constant.character.comfy`（Alive 只把 `'` 當 quote）

> 字串內的 `\n \t …` 轉義**不用這裡管**——Alive 的字串文法已有 `\\.` → `constant.character.escape`，
> 本來就會上色。

注入目標：`source.lisp`（Alive 的 Common Lisp base scope）。scope 開頭是標準的 `constant.language`／
`constant.character`，主題對這兩類本就上色，故通常跟其他常數／字元同色。

## 啟用

已由安裝腳本複製到 `%USERPROFILE%\.vscode\extensions\local.vscode-comfy-0.0.1\`。
改動文法後要重裝：把本資料夾內容覆蓋到該處，再 **Developer: Reload Window**。

也可不裝全域、開發宿主試跑：VSCode 開本資料夾 → `F5` → 在新視窗開 `.lisp` 檔。

## 校準（若沒生效）

1. `.lisp` 檔裡游標停在 token 上 → 命令面板 **Developer: Inspect Editor Tokens and Scopes**，
   確認 base scope 是不是 `source.lisp`、字串是不是 `string.quoted.double.commonlisp`。
2. 若 Alive 的 **LSP 語意高亮**蓋過本擴充（連上 REPL 後才有），可在 settings 關掉該語言的
   semantic highlighting，或用 `editor.tokenColorCustomizations` 對 `*.comfy` scope 指定顏色。
3. 顏色由主題決定；本擴充只指派 scope、不硬塞顏色。

> ⚠ TextMate 上色無法在編輯器外驗證，實際效果請在 VSCode 眼睛確認。
