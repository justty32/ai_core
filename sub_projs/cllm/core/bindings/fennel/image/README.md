# Fennel 映像：產出 · 利用（CLI/lib）· 執行期修改

← [bindings](../../README.md)｜需先 `source ~/dev/cllm/env.sh`

> **可行性結論（你問「可以的話」）**：Fennel 跑在 Lua 上，**Lua 沒有記憶體映像 dump**（不像 SBCL 存不了活堆疊）。所以 Fennel 的「image」＝**AOT 編成獨立 `.lua`**（甚至再打成單一 binary），而 C 綁定 `llm.so` 永遠是執行期才載入的共享庫——**烤不進映像**。三件事做得到的都實測過、做不到的誠實標明。

## 1. 產出映像（＝AOT 編成獨立 .lua）

```bash
source ~/dev/cllm/env.sh
fennel --compile app.fnl > app.lua      # Fennel → 純 Lua（本機實測 3 行）
```
產出的 `app.lua` **免 fennel、只要 lua + llm.so** 就能跑（見下）。要更「單一檔」可選 `luastatic`（`luarocks install luastatic`）把 lua runtime＋腳本打成一顆執行檔——但 `llm.so`（C 綁定）仍是動態載入。

## 2. 利用映像

**當 CLI（獨立 .lua，免 fennel）**：
```bash
lua5.4 app.lua "$CLLM_FIXTURES/"        # 純 lua 直接跑編出來的映像
```
**當 lib**：編出的 `.lua` 就是普通 Lua 模組，別的 Lua/Fennel 專案 `require` 它即可；或用 `fennel.eval` 在執行期把 Fennel 源碼編了就用。

## 3. 執行期修改程式

```bash
fennel runtime-modify.fnl
```
[runtime-modify.fnl](runtime-modify.fnl)：早定義的 `caller`，重綁 global `greet` 後**立即走新版**（Lua 函式是值、global 呼叫在執行期查找）。

**互動玩法**：`fennel`（不給檔）進 REPL，貼新 `(fn …)` 或用 `,reload <mod>` 熱更模組；嵌入端用 `fennel.eval` 灌新碼。

> ⚠ **取捨**：執行期修改是活的，但**存不成映像**（Lua 限制）——固化只能「重存原始碼再 AOT 編」。對照：SBCL 能把跑著改出來的狀態 `save-lisp-and-die` 存成新映像（見 [../../lisp/image/](../../lisp/image/README.md)）；s7 能 bake 成獨立執行檔但同樣無 dump（見 [../../s7/image/](../../s7/image/README.md)）。

## 三語言映像能力速覽

| | 記憶體映像 dump | 產出獨立執行檔 | 執行期修改 | 改動可存回映像 |
|---|---|---|---|---|
| **Common Lisp (SBCL)** | ✅ `save-lisp-and-die` | ✅ exe/core | ✅ | ✅（再 save 即固化）|
| **s7** | ❌ | ✅ 烤進 C host | ✅（活 REPL）| ❌（重編）|
| **Fennel/Lua** | ❌ | ✅ AOT `.lua`(+luastatic) | ✅（活 REPL）| ❌（重編）|
