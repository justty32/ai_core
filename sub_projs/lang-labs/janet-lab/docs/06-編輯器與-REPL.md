# 06 · 編輯器與 REPL

nvim 已配好，跟你原本 hy / fennel / scheme 同一套 Conjure 工作流。

## 已經設好的東西

| 功能 | 外掛 | 狀態 |
|------|------|------|
| REPL 求值（`,ee` 等） | Conjure（janet stdio client） | ✅ 開檔即自動起 `janet -n -s` |
| 括號結構編輯 | parinfer-rust | ✅（`janet` 本來就在 ft 清單） |
| 彩虹括號 | rainbow-delimiters | ✅（同上） |
| 語法高亮 | treesitter `janet_simple` parser | ✅（`janet.lua` 已加進 `ensure_installed`） |
| LSP（補全 / hover / 診斷） | janet-lsp | ⚠ 已裝二進位，nvim 端**尚未接**（見下） |

設定檔：`~/.config/nvim/lua/plugins/janet.lua`。

## Conjure REPL 工作流（跟 scheme 一樣）

開任何 `.janet` 檔，Conjure 會在背景自動起一個 janet REPL 子行程，然後：

| 鍵 | 動作 |
|----|------|
| `,ee` | 求值游標所在的 form |
| `,er` | 求值游標所在的**最外層** form |
| `,eb` | 求值整個 buffer |
| `,ls` / `,lv` | 開 REPL log（橫向 / 直向） |

「REPL 一直開、邊寫邊試」——寫一個 `defn` 就 `,ee` 送進 REPL，馬上在 log 看結果。

> 小提醒：Conjure 對 janet 的**預設** client 是 netrepl（要另起伺服器），這裡已在
> `janet.lua` 改成 stdio（開檔自帶 REPL、零設定）。為什麼設定寫在檔案 body 而不是 spec 的
> `init`，`janet.lua` 註解裡有寫（多個檔共用同一個 conjure plugin，`init` 會被後載入者蓋掉）。

## 若要接 janet-lsp（可選）

`janet-lsp` 二進位已裝在 `~/.local/bin/janet-lsp`，`nvim-lspconfig` 也內建 `janet_lsp` 設定，
但因為你其它 lisp（fennel/hy/scheme）都走 Conjure、沒接 LSP，這裡**先不自動開**，避免跟你既有
習慣衝突。想試的話，新增一個 plugin 檔：

```lua
-- ~/.config/nvim/lua/plugins/janet-lsp.lua
return {
  {
    "neovim/nvim-lspconfig",
    opts = function(_, opts)
      opts.servers = opts.servers or {}
      opts.servers.janet_lsp = {}   -- 二進位在 PATH 上，lspconfig 直接抓
    end,
  },
}
```

LSP 給的是補全 / 跳定義 / 即時診斷，跟 Conjure 的 REPL 求值互補、可並存。

## VSCode（如果哪天想用）

裝擴充套件 **"Janet" / "janet-lang"**，並可搭 janet-lsp。不過你主力是 nvim，這裡不展開。
