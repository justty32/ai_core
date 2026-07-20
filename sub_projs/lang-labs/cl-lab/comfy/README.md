# comfy — 舒適 Common Lisp 地基

← [sub_projs](../README.md)

一個**動手實驗場**（跟 [galtxt/try_1](../galtxt/try_1/README.md) 同性質，**刻意不套**九軸／workflow 框架）。
目的兩個：

1. **一層薄薄的順手糖**，蓋在標準 Common Lisp 上，把幾個我不習慣的寫法換成舒服的——
   目前：`true`/`false` 代 `t`/`nil`、`'a'` 代 `#\a`、**C 風格字串轉義**（`"a\nb"` 含真換行）。
   數字與其餘資料形式本就與 C 一致，不動。
2. **一個成熟、能在 VSCode 裡真 debug 的開發環境**：SBCL ＋ Alive 擴充 ＋ 內建 ASDF，
   REPL 邊寫邊測、condition/restart 互動除錯、`trace`/`step`/`break` 全在手邊。

> **⚠ 在 AGENTS.md 鐵律之外**：專案鐵律定「實作只用 Python 3.11+ 標準庫、POSIX、Windows 不考慮」。
> 本夾與 try_1 一樣是**框架外的實驗場**（Common Lisp / SBCL / Windows），故**不受該鐵律約束**。
> 相依原則：**糖本體（`:comfy` 系統）維持零外部相依**（只用 SBCL 內建 ASDF）；至於真實工作需要的
> 庫（如 JSON），走 **Quicklisp** 引入——galtxt 的 LLM 接口一定要 JSON，故已裝 Quicklisp ＋
> `com.inuoe.jzon`（見「JSON」節）。糖與庫分層：載入糖不會拖進任何外部相依。

## 由來

s7 Scheme（見 [galtxt/try_1](../galtxt/try_1/README.md)）跑通了 LLM 接口，但語法不順手、且沒有成熟的
VSCode 除錯環境。比過 Racket（VSCode step-debug 偏弱）、Clojure（JVM 太重）後，選定 **SBCL＝
普通 Common Lisp**：debugger／condition system 真成熟，Alive 給得起 VSCode 裡的 REPL/inline-eval/
互動除錯，而 `true`/`false` 用 `defconstant` 就有、`'a'` 用 reader macro 就有，都不必魔改語意。

與 s7 那條線**並存、兩邊都推**。

## 需要什麼

- **SBCL**（Windows 端 winget 裝在 `C:\Program Files\Steel Bank Common Lisp\sbcl.exe`，實測 2.6.6）。ASDF 內建。
- **VSCode ＋ Alive 擴充**（`rheller.alive`）＋ **alive-lsp 的相依**（見「跨平台」）。
- **Quicklisp**（`~/quicklisp`，`~/.sbclrc` 設自動載入）＋ JSON 庫 **`com.inuoe.jzon`**（見「JSON」節）。

## 跨平台（Windows ⇄ Manjaro）

本 sub-proj 在**兩台機器共用同一 repo**：公司 Windows、家裡 Manjaro。原始碼（`.lisp`／糖／測試）
純可攜；只有**工具鏈與絕對路徑**是本機的，處理如下：

- **`.vscode/settings.json` 已 gitignore**（含絕對 sbcl 路徑、本機專用）。各機**從模板複製**：
  - Windows： `cp .vscode/settings.windows.jsonc .vscode/settings.json`
  - Manjaro： `cp .vscode/settings.linux.jsonc  .vscode/settings.json`
  - 差別只有 `startCommand` 的 `command[0]`：Windows 用 sbcl.exe 絕對路徑（Node spawn 不補 `.exe`），
    Linux 直接 `sbcl`（在 PATH）。其餘序列一致。
- **`~/.sbclrc`／`~/quicklisp`**：本機層，不在 repo。用 `(user-homedir-pathname)` 寫路徑故可攜。

**Manjaro 首次設定清單**（回家照做一次）：

```sh
sudo pacman -S sbcl                                   # 1. SBCL
# 2. Quicklisp
curl -O https://beta.quicklisp.org/quicklisp.lisp
sbcl --non-interactive --load quicklisp.lisp --eval '(quicklisp-quickstart:install)'
#    把載入片段寫進 ~/.sbclrc（#-quicklisp (load ~/quicklisp/setup.lisp)）
# 3. alive-lsp 的相依（否則 Alive 的 LSP 起不來）
sbcl --non-interactive --eval '(ql:quickload (list :usocket :cl-json :bordeaux-threads :flexi-streams))'
# 4. VSCode 裝 Alive 擴充（rheller.alive）
# 5. comfy 高亮擴充
cp -r editor/vscode-comfy ~/.vscode/extensions/local.vscode-comfy-0.0.1
# 6. 複製 Linux 版 settings 模板
cp .vscode/settings.linux.jsonc .vscode/settings.json
```

驗證：`sbcl --script test/run.lisp` 應印 `ALL GREEN`（純可攜，兩平台都該綠）。

## 佈局

| 路徑 | 是什麼 |
|------|--------|
| `comfy.asd` | ASDF 系統定義（`(asdf:load-system :comfy)`）|
| `src/package.lisp` | 套件 `:comfy` 與匯出清單 |
| `src/comfy.lisp` | 糖本體：`true`/`false` 常數、`'a'` 字元讀取器、C 風格字串讀取器、`*comfy-readtable*` |
| `test/run.lisp` | 無框架 sanity 測試（`sbcl --script test/run.lisp`）|
| `examples/hello.lisp` | 糖示範 |
| `examples/json-demo.lisp` | jzon（Quicklisp）JSON round-trip 示範 |
| `.vscode/settings.{windows,linux}.jsonc` | Alive 設定的兩平台模板（複製成 `.vscode/settings.json`，見「跨平台」）|
| `editor/vscode-comfy/` | 讓 comfy 糖在 VSCode 正確上色的**注入文法小擴充**（見「語法高亮」節）|

## 舒適糖用法

**布林**——`import` 或 `use` 了 `:comfy` 就能寫 `true`/`false`（就是 `t`/`nil` 的別名常數）：

```lisp
(if true 42 0)        ; => 42
(remove-if #'null '(a false b))  ; false 就是 nil
```

**字元 `'a'`**——要在檔頭啟用 comfy readtable（因為 `'` 本來是 quote，得換讀取器來分辨）：

```lisp
(eval-when (:compile-toplevel :load-toplevel :execute)
  (comfy:enable-comfy-syntax))

'a'        ; => #\a
'\n'       ; => #\Newline      （轉義： \n \t \r \a \b \f \v \0 \s(空白) \\ \'）
'x         ; => (quote x)      ← 一般 quote 完全不變
'(1 2 3)   ; => (quote (1 2 3))
```

分辨規則：讀到 `'` 後往下探一格——**後面緊跟收尾 `'` 就是字元**（`'a'`），否則**退回標準 quote**
（`'x`、`'(…)`）。只裝在 `*comfy-readtable*`，不污染全域 `*readtable*`。

**C 風格字串**——同樣需啟用 comfy readtable。標準 CL 字串裡反斜線**只** escape `"` 與 `\`，
所以 `"a\nb"` 在 CL 是字面的 `a n b`（不含換行），C 慣用者會踩坑。comfy 把 `"…"` 換成 **C 語意**：

```lisp
"a\nb"          ; => 三字元字串： a、換行、b
"tab\there"     ; => \t 是真 Tab
"say \"hi\""    ; => \" → "（同 C，也同標準 CL）
"back\\slash"   ; => \\ → 單一反斜線
"\x41\x42"      ; => "AB"（\xHH 兩位十六進位）
```

支援 `\n \t \r \a \b \f \v \0 \\ \" \'` 與 `\xHH`；其餘 `\X` → `X`。**數字不動**（`42`、`3.14`、
`#xFF` 都照標準 CL 讀）。字串 reader 一樣只裝在 `*comfy-readtable*`。

## JSON（Quicklisp ＋ jzon）

CL 無標準庫 JSON，用成熟庫 **[`com.inuoe.jzon`](https://github.com/Zulu-Inuoe/jzon)**（純 CL、快、無 C 依賴）。
已裝好 Quicklisp（`~/quicklisp`）與 jzon，`~/.sbclrc` 會自動載入 Quicklisp——**REPL/Alive 裡直接**：

```lisp
(ql:quickload :com.inuoe.jzon)
(com.inuoe.jzon:parse "{\"msg\":\"你好\",\"n\":42,\"ok\":true}")  ; => hash-table（中文原樣）
(com.inuoe.jzon:stringify ht)                                     ; CL 資料 → JSON 字串
```

對照表：JSON object→`hash-table`（鍵字串）、array→`vector`、true/false→`t`/`nil`、null→`null`（符號）、
number→整數或 `double-float`。完整示範見 [`examples/json-demo.lisp`](examples/json-demo.lisp)
（`sbcl --script examples/json-demo.lisp`，實測解析中文＋round-trip＋用 comfy 的 `true` 組 JSON 全對）。

> `--script` 不讀 `~/.sbclrc`，故腳本裡要手動 `(load (merge-pathnames "quicklisp/setup.lisp" (user-homedir-pathname)))`；
> 互動的 Alive/REPL 則自動載入，免手動。

## 語法高亮（VSCode）

VSCode 高亮是**靜態 TextMate 文法**，跟 comfy 的 reader macro（執行期）是兩套系統，所以糖預設不會被
特別上色。用一個**注入文法小擴充**（[`editor/vscode-comfy/`](editor/vscode-comfy/README.md)）補兩項：
`true`/`false`（Alive 只認 `t`/`nil`）、`'a'` 字元（Alive 只把 `'` 當 quote）。**字串轉義 `\n` 等 Alive
本來就上色**，不用管。

已安裝到 `%USERPROFILE%\.vscode\extensions\local.vscode-comfy-0.0.1\`——**命令面板 → Developer: Reload
Window** 即生效。改文法後重裝＝覆蓋該處再 reload。scope 開頭是標準 `constant.language`／`constant.character`，
主題本就上色。

> ⚠ TextMate 上色我無法在編輯器外驗證，實際效果請你眼睛確認；沒生效就用「Developer: Inspect Editor
> Tokens and Scopes」查真 scope 回報。若 Alive 的 LSP 語意高亮蓋過，見擴充 README 的校準節。

## 跑起來

```powershell
# 測試（全綠印 ALL GREEN、exit 0）
sbcl --script test/run.lisp

# 範例
sbcl --script examples/hello.lisp
```

（Git Bash 裡 sbcl 若不在 PATH： `"/c/Program Files/Steel Bank Common Lisp/sbcl.exe" --script test/run.lisp`。）

## 在 VSCode 裡開發／debug（Alive）

> **首次設定的兩個坑（已排除，跨機重現時注意）**：
> 1. **Alive LSP 的相依**：alive-lsp 需要 `usocket` / `cl-json` / `bordeaux-threads` / `flexi-streams`
>    （＋內建 `sb-introspect`）。用 Quicklisp 下載一次即可：
>    `(ql:quickload (list :usocket :cl-json :bordeaux-threads :flexi-streams))`。沒裝的話 LSP 一啟動就
>    因 `Component USOCKET not found` 掛掉、REPL 進不去。
> 2. **`alive.lsp.startCommand` 是完整啟動指令**（Alive 逐字執行、讀 stdout 抓 port），必須自己
>    `load-system :alive-lsp` 並 `(alive/server:start)`——只填 sbcl 執行檔會啟不了 server。本夾
>    `.vscode/settings.json` 已寫好正確序列（實測印出 `Started on port …`）。
> 3. **別把 `*.lisp` 綁成語言 `lisp`**——Alive 用的是 `commonlisp`，綁錯文法與 LSP 都掛不上。

1. 用 VSCode **開 `comfy/` 這個資料夾**（`.vscode/settings.json` 才會生效）。
2. 命令面板 → **Alive: Start REPL And Attach**（依上面的 startCommand 起 LSP＋REPL）。
3. 載入系統：REPL 打 `(asdf:load-system :comfy)`，或對 `comfy.asd`／原始碼用 Alive 的
   **Load File**、**inline eval**（游標停在某個 form 上按 eval 快捷鍵，結果就地顯示）。
4. **除錯手段**（這就是「成熟環境」的本體）：
   - **inline eval / REPL**：任何 form 當場求值看結果——Lisp 開發的核心循環。
   - `(trace 函式名)`：進出參數與回傳全印出來。
   - `(step (你的表達式))`：單步。
   - `(break "訊息")`：埋中斷點；命中時進**互動除錯器**——Alive 會把 backtrace 與可選的
     **restart** 攤在面板上，你選一個 restart 就能就地恢復／重試，不必重跑整支程式。
   - 出錯時的 **condition/restart** 本身就是除錯器：signal → 一串 restart 供你選，這是 CL 比
     一般語言強的地方。

## 目前狀態 / 下一步

- ✅ SBCL＋Alive 環境就緒；`:comfy` 系統可載入；`true`/`false`＋`'a'` 字元＋C 風格字串轉義測試全綠。
- ✅ Quicklisp ＋ jzon 裝好，JSON round-trip 實測通過（`examples/json-demo.lisp`）。
- ✅ VSCode 注入文法擴充備妥（`editor/vscode-comfy/`）——**待你在 VSCode 眼睛確認上色**。
- 之後想加的糖（等實際用到再長）：更多順手別名、字串／格式小工具；
  以及把 s7 那邊「schema→CLI 旗標由同像性生成」的洞見，在 CL 這邊用 macro 重生。
