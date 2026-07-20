# cllm-lab — 十語言 LLM 開發遊樂場

用任何語言呼叫 LLM（`libcllm.so` 的 C ABI，唯一入口 `llm_ask`）。每個資料夾＝一個語言，
`play.*` 是完整範例起手檔（基本 ask／串流／schema 結構化輸出／tools／media／modalities 全示範），
**直接改它就是開發**。

> 📖 **要教學／速查？** 完整上手教學看 [`GUIDE.md`](GUIDE.md)（心智模型→CLI→九語言 API→接後端登入→建置→疑難排解）；
> 一頁速查、可印可貼牆的版本 → 瀏覽器打開 [`cheatsheet.html`](cheatsheet.html)（離線可用、深淺色自適應）。

## 30 秒開工

```bash
source ~/dev/cllm/env.sh      # ① 先設環境（PATH／pkg-config／各語言路徑）——開編輯器前做
cd cpp && bash run.sh    # ② 挑個語言跑跑看（離線 fixture，不用開後端）
code . 或 nvim play.cpp  # ③ 開始改
```

- **`run.sh`**：自動 source env、編譯（如需要）、預設打離線假回應（`$CLLM_FIXTURES`）。
- **接真後端**：改 `play.*` 裡的 endpoint 為 `http://localhost:1234/v1/chat/completions`（LM Studio）。
- ⚠ **編輯器要從 source 過 env.sh 的 shell 開**——clangd／gopls 靠環境變數找標頭與 pkg-config。

## 各語言

| 資料夾 | 起手檔 | 跑 | 編輯器支援 |
|--------|--------|-----|-----------|
| [c/](c/) | play.c | `bash run.sh` | clangd（`compile_flags.txt` 已備）|
| [cpp/](cpp/) | play.cpp | `bash run.sh` | clangd（同上；有便利層 `<cllm/llm.hpp>`＋反射糖 `llm_reflect.hpp`）|
| [lua/](lua/) | play.lua | `bash run.sh` | lua-ls（`.luarc.json` 已備）|
| [fennel/](fennel/) | play.fnl | `bash run.sh` | —（跑在 lua 5.4；⚠ table 鍵用底線 `:on_delta`）|
| [s7/](s7/) | play.scm | `bash run.sh` | —（⚠ 回呼內別丟 error——longjmp 穿 C++ 是 UB）|
| [python/](python/) | play.py | `bash run.sh` | pyright（`pyrightconfig.json` 已備）|
| [janet/](janet/) | play.janet | `bash run.sh` | janet-lsp（`project.janet` 已備）；native C 模組，JSON 用 spork/json |
| [lisp/](lisp/) | play.lisp | `bash run.sh` | Swank/SLIME 照常；需 quicklisp（CFFI＋shasht）|
| [go/](go/) | main.go | `bash run.sh` | gopls（`go.mod` replace 指到 ~/dev；cgo 需先 source env.sh）|
| [shell/](shell/) | play.sh | `bash run.sh` | —（`llm` CLI 是 unix filter）|

API 三句話：`ask(prompt[, endpoint], opts…)` 回完整答案字串；`on_delta` 逐段串流；欄位
`system`（system role 指示）＋進階 `tools`／`on_tool`／`media`／`on_media`／`modalities`（各語言慣例命名）。C++ 另有
`std::expected` 便利層＋`ask_as<T>`（struct 進 struct 出）。

## 出了問題／要查細節

- **環境壞了？** 重建：`bash ~/repo/ai_core/sub_projs/cllm/core/install-dev.sh`（冪等覆蓋 ~/dev 的 cllm 部分）
- **環境健康檢查**：`bash ~/repo/ai_core/sub_projs/cllm/core/test/bindings_smoke.sh`（九語言一鍵，全綠＝正常）
- **完整文檔**（選項表／注意事項／C ABI 參考）：`~/repo/ai_core/sub_projs/cllm/core/bindings/README.md`
- **真相源**：本資料夾的 `play.*` 抄自 `~/repo/ai_core/sub_projs/cllm/core/bindings/<lang>/example.*`——
  lab 是暫存遊樂場，**值得留的成果記得搬回 repo**。

## Windows（MinGW）註記

- **C++ 庫路徑已在 Windows 驗綠**（2026-07-20，mingw64 g++16.1.0）：`cpp/play.cpp` 的六大能力
  （ask／stream／`ask_as<T>` 反射／tool／media out／media in）連結 `libcllm.dll.a`＋glaze 後全對。
  但**繞過了 `install-dev.sh`／pkg-config**（手工組 include layout＋直接 `-I…/src -I…/glaze -L…/build -lcllm`）——
  Windows 沒 pkg-config，`run.sh` 那條走不了；打包層（`~/dev` 前綴）在 Windows 尚未驗。
- **`play.cpp` 第 7 步（shell-out 呼 `llm` CLI）在 Windows 失敗**、非 cllm 問題：那句 `popen("llm … --endpoint 'file://…'")`
  用單引號，Windows `popen`／`system` 走 `cmd.exe`、單引號非引號 → llm 收到帶引號 endpoint → URL 解析失敗。
  同 cpp-handy 的 POSIX-shell 假設坑（見 cllm [gotchas/windows](../core/workflows/common/gotchas/windows.md)）。
