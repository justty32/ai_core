# comfy — 舒適 Common Lisp 地基

← [sub_projs](../README.md)

一個**動手實驗場**（跟 [galtxt/try_1](../galtxt/try_1/README.md) 同性質，**刻意不套**九軸／workflow 框架）。
目的兩個：

1. **一層薄薄的順手糖**，蓋在標準 Common Lisp 上，把幾個我不習慣的寫法換成舒服的——
   目前：`true`/`false` 代 `t`/`nil`、`'a'` 代 `#\a`（含 `'\n'` 這類轉義）。
2. **一個成熟、能在 VSCode 裡真 debug 的開發環境**：SBCL ＋ Alive 擴充 ＋ 內建 ASDF，
   REPL 邊寫邊測、condition/restart 互動除錯、`trace`/`step`/`break` 全在手邊。

> **⚠ 在 AGENTS.md 鐵律之外**：專案鐵律定「實作只用 Python 3.11+ 標準庫、POSIX、Windows 不考慮」。
> 本夾與 try_1 一樣是**框架外的實驗場**（Common Lisp / SBCL / Windows），故**不受該鐵律約束**；
> 但仍守「零外部相依」的精神——只用 SBCL 隨附的 ASDF，不碰 Quicklisp（有真依賴再說）。

## 由來

s7 Scheme（見 [galtxt/try_1](../galtxt/try_1/README.md)）跑通了 LLM 接口，但語法不順手、且沒有成熟的
VSCode 除錯環境。比過 Racket（VSCode step-debug 偏弱）、Clojure（JVM 太重）後，選定 **SBCL＝
普通 Common Lisp**：debugger／condition system 真成熟，Alive 給得起 VSCode 裡的 REPL/inline-eval/
互動除錯，而 `true`/`false` 用 `defconstant` 就有、`'a'` 用 reader macro 就有，都不必魔改語意。

與 s7 那條線**並存、兩邊都推**。

## 需要什麼

- **SBCL**（本機 winget 裝在 `C:\Program Files\Steel Bank Common Lisp\sbcl.exe`，實測 2.6.6）。
- **VSCode ＋ Alive 擴充**（`rheller.alive`，已裝）。ASDF 隨 SBCL 內建，免另裝。

## 佈局

| 路徑 | 是什麼 |
|------|--------|
| `comfy.asd` | ASDF 系統定義（`(asdf:load-system :comfy)`）|
| `src/package.lisp` | 套件 `:comfy` 與匯出清單 |
| `src/comfy.lisp` | 糖本體：`true`/`false` 常數、`'a'` 字元讀取器、`*comfy-readtable*` |
| `test/run.lisp` | 無框架 sanity 測試（`sbcl --script test/run.lisp`）|
| `examples/hello.lisp` | 糖示範 |
| `.vscode/settings.json` | Alive 設定（指到上面的 sbcl.exe）|

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
'\n'       ; => #\Newline      （轉義： \n \t \r \s(空白) \0(NUL) \\ \'）
'x         ; => (quote x)      ← 一般 quote 完全不變
'(1 2 3)   ; => (quote (1 2 3))
```

分辨規則：讀到 `'` 後往下探一格——**後面緊跟收尾 `'` 就是字元**（`'a'`），否則**退回標準 quote**
（`'x`、`'(…)`）。只裝在 `*comfy-readtable*`，不污染全域 `*readtable*`。

## 跑起來

```powershell
# 測試（全綠印 ALL GREEN、exit 0）
sbcl --script test/run.lisp

# 範例
sbcl --script examples/hello.lisp
```

（Git Bash 裡 sbcl 若不在 PATH： `"/c/Program Files/Steel Bank Common Lisp/sbcl.exe" --script test/run.lisp`。）

## 在 VSCode 裡開發／debug（Alive）

1. 用 VSCode **開 `comfy/` 這個資料夾**（`.vscode/settings.json` 才會生效）。
2. 命令面板 → **Alive: Start REPL And Attach**（會用設定裡的 sbcl 起一個 REPL 進程）。
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

- ✅ SBCL＋Alive 環境就緒；`:comfy` 系統可載入；`true`/`false`＋`'a'`（含轉義）測試全綠。
- 之後想加的糖（等實際用到再長）：更多順手別名、字串／格式小工具；
  以及把 s7 那邊「schema→CLI 旗標由同像性生成」的洞見，在 CL 這邊用 macro 重生。
