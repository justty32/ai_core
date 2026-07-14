# vscode-comfy — comfy 語法糖高亮（注入文法）

VSCode 的語法高亮是**靜態 TextMate 文法**在跑，跟 comfy 的 reader macro（**執行期**才改變讀取）
是兩套系統，所以 comfy 的糖預設不會被正確上色：`true`/`false` 被當普通符號、`'a'` 被當 quote 誤上色、
字串裡的 `\n` 不被標成轉義。這個小擴充用**注入文法**（injection grammar）補上，不改 Lisp 語言本身。

上色規則：
- `true` / `false` → `constant.language.boolean.comfy`
- `'a'`、`'\n'` 等字元字面量 → `constant.character.comfy`
- 字串內 `\n \t … \xHH \\ \" \'` → `constant.character.escape.comfy`

## 啟用（三選一）

1. **開發宿主試跑（最快，不裝進全域）**：用 VSCode 開 `sub_projs/comfy/editor/vscode-comfy/`，按 `F5`
   起 Extension Development Host，在那個視窗開 `.lisp` 檔看效果。
2. **裝進全域**：把整個 `vscode-comfy/` 資料夾複製（或 symlink）到 `%USERPROFILE%\.vscode\extensions\`，
   重啟 VSCode。
3. 不想裝就算了——高亮不影響執行，comfy 的糖照樣跑。

## 校準（重要）

TextMate 的 scope 名稱依你實際用的 Lisp 文法而定（內建 `.lisp`＝`source.lisp`；Alive 的
Common Lisp 可能是 `source.commonlisp`）。若某條沒生效：

- 在 `.lisp` 檔裡把游標停在該 token 上 → 命令面板 **Developer: Inspect Editor Tokens and Scopes**，
  看它真正的 scope（例如字串是不是 `string.quoted.double.lisp`），再對照本擴充的 `injectionSelector`／
  `injectTo` 調整。
- 顏色本身由你的主題決定（本擴充只指派 scope，不硬塞顏色）。想指定色，用 `editor.tokenColorCustomizations`
  的 `textMateRules` 對上面那幾個 `*.comfy` scope 上色。

> ⚠ 這份是**第一版、未經渲染驗證**——文法 JSON 結構正確，但實際上色效果請你在 VSCode 裡眼睛確認、
> 必要時照上面校準。
