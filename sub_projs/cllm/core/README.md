# cllm — C LLM：對外 C ABI（`libcllm.so`）＋ `llm` CLI（unix filter）

← [sub_projs](../README.md)

**cllm** 把 LLM 收成**一支對外 C ABI 共享庫 `libcllm.so`**（`extern "C"`，唯一入口 `llm_ask`）＋建在其上的 **`llm` unix filter CLI**。源自 galtxt 的純 C++ 實驗線（原 `galtxt/try_3`），收斂成兩交付物後**抽離成獨立產物**。

**完全 C++、純原生**——不嵌任何腳本 VM。（早期曾用 C++20 modules 當骨架示範，**已回歸傳統 header**：C++ 有 struct＋glaze 編譯期反射，接口的唯一真相源是 struct 本身，modules 那層抽象畫蛇添足，拿掉。）建置走 **CMake + Ninja + vcpkg toolchain**（配 `CMakePresets.json` 釘死工具鏈）。

目前狀態：**可建置、可 VSCode/gdb 除錯；Windows／Linux（Manjaro）雙機皆已實測**。

> 📖 本 README 是**敘事型技術入口**。要查逐型別／逐欄位／逐旗標的**正式 API 參考** → [`docs/`](docs/README.md)，或先看 [`docs/overview.html`](docs/overview.html) **一頁看懂**。

> ★ **2026-07 收斂成兩個對外交付物**（早期那批可獨立呼叫的 L0 C++ 入口＋demo exe 已刪除，史蹟在 git log 的 `archived/`）：
> 1. **`libcllm.so` —— 對外 C ABI**（`src/cabi.*`，`extern "C"`）：唯一入口 `llm_ask` 統一吃 prompt＋schema＋tools＋media＋modalities＋stream。純 C 客戶端 include `cabi.h`、連 `-lcllm` 即用；另附 header-only 的 C++ 薄鏡像 `cabi.hpp`（`llm::abi`）。
> 2. **`llm` —— unix filter CLI**（`src/cli.*`＋`src/main.cpp`）：走 `cabi.hpp` 消費 `libcllm`。

現行原始碼（`src/`）：`http.{hpp,cpp}`（傳輸）＋`cabi*.h`／`cabi*.cpp`（C ABI 傘檔＋功能頭＋按關注點拆的實作）＋`cabi*.hpp`（C++ 薄鏡像）＋`cli.{hpp,cpp}`＋`main.cpp`（`llm` CLI）。**逐檔領域／關鍵符號** → [common/code-map](workflows/common/code-map/CODE_MAP.md)。（舊 L0 `archived/` 已於 2026-07-16 刪除——內容早已融進 C ABI 實作，查史用 `git log -- sub_projs/cllm/archived`。）

離線 fixture（`test/fixtures/{fake,fake_stream,fake_tool,fake_json,fake_media}/`）＋ `test/cli_smoke.sh` 端到端驗 `llm` CLI（35/35 綠）＋ `test/bindings_smoke.sh` 一鍵驗九語言綁定（輪流呼叫各 `bindings/<lang>/smoke.sh`）。

## 建置（命令列）

環境需求／vcpkg 依賴 → [docs/setup.md](docs/setup.md)；跨平台 preset → [docs/platform.md](docs/platform.md)。

```bash
# Windows（mingw-debug／mingw-release）
cmake --preset mingw-debug          # 首次 configure（讀 CMakePresets.json）
cmake --build --preset mingw-debug  # 建置 → build/libcllm.so ＋ build/llm.exe
./build/llm.exe 你好                # 跑 CLI（Windows 產物帶 .exe）

# Linux（linux-debug／linux-release，指令完全同構）
cmake --preset linux-debug
cmake --build --preset linux-debug  # 建置 → build/libcllm.so ＋ build/llm
./build/llm 你好                    # Linux 產物無副檔名（unix filter 慣例）
bash test/cli_smoke.sh              # 離線黑箱煙霧測試（驅動 build/llm 打 file:// fixtures）
```

Release：把 `-debug` 換成 `-release`。`cmake --list-presets` 只列當前平台的 preset（兩組各掛 `hostSystemName` condition，不用手動挑）。

> ⚠ **CLI 產物名跨平台不同**：`.so` 兩邊都叫 `libcllm.so`；`llm` 執行檔在 Windows 是 `llm.exe`、Linux 是 `llm`（無副檔名）——CLI 是 unix filter，Linux 側刻意**不**加 `.exe`。

## 兩交付物（技術全貌見 docs）

**① 對外 C ABI（`libcllm.so`，`src/cabi.*`）**：在 http＋glaze 兩塊地基上，把 LLM 接口收成 `extern "C"` 扁平結構＋**唯一入口 `llm_ask`**——一次吃 prompt＋schema＋tools＋media＋modalities＋stream，組成**一個** OpenAI 請求，輸出走對稱四 handler（`on_text`／`on_tool`／`on_media`／`on_error`）。為什麼一個入口而非四個：舊 L0 的 `ask`/`ask_tools`/`ask_vision`/`ask_as` 彼此不可組合（要「串流的結構化輸出＋帶圖」就沒轍）；整合搬進唯一一處 `build_body`，四塊變正交欄位、隨意組合。另附 header-only C++ 薄鏡像 `cabi.hpp`（`llm::abi`，`const char*`→`std::string`、`field_mask`→`std::optional`、函數指標→`std::function`）。

> **逐型別／欄位／enum 參考**：[C ABI](docs/c-abi-reference.md)（[輸入](docs/c-abi-input.md)／[輸出](docs/c-abi-output.md)）｜[C++ 鏡像](docs/cpp-mirror-reference.md)。實作按關注點拆檔的分工表在 [`src/cabi_internal.hpp`](src/cabi_internal.hpp)／[code-map](workflows/common/code-map/CODE_MAP.md)。

**② `llm` CLI（unix filter，`src/cli.*`＋`src/main.cpp`）**：走 C++ 鏡像 `cabi.hpp` 消費 `libcllm`。prompt 走位置參數、沒給就讀 stdin；答案吐 stdout、診斷吐 stderr。**連線／取樣旗標從 `llm::abi::Client` 欄位反射生成**（`glz::for_each_field`＋`reflect::keys`；加欄位＝旗標與 `--help` 自動長出來），固定旗標（`--system`／`--stream`／`--image`／`--schema`／`--config`／`--help`）手寫。設定三層來源：內建預設 → config 檔 → 命令列旗標。

```bash
llm 用一句話介紹你自己                      # 位置參數當 prompt
echo "數到五" | llm --stream               # 讀 stdin；--stream 逐段即時印
llm 這張圖是什麼 --image ./cat.jpg          # --image 可重複
llm 生成一個傲嬌角色 --schema ./char.json   # JSON Schema 檔 → 結構化輸出
```

> **完整旗標表／config 三層來源／退出碼（0/1/2/130）／離線自測** → [CLI 手冊](docs/cli-manual.md)。

## 接真後端（本機 LM Studio）

離線 fixture 是**回歸測試**（`test/cli_smoke.sh` 全走 `file://`、不連網）。要打**真的**本機 LM Studio，把 `llm` 的 `--endpoint` 指過去（或寫進 config 檔）：

```bash
llm 用一句話介紹你自己 --endpoint http://localhost:1234/v1/chat/completions --model local-model
# 或寫進 config（~/.config/llm/config.json 或 --config／env LLM_CLI_CONFIG 指定）之後裸跑：
#   { "endpoint": "http://localhost:1234/v1/chat/completions", "model": "local-model", "timeout_ms": 120000 }
```

- **`endpoint`**：到 `/v1/chat/completions` 為止的完整 URL。**`model`**：LM Studio 接受 `local-model`（用已載入模型）；雲端 API 需真實 model id＋`api_key`。**`timeout_ms`**：真後端（尤其 reasoning）單次可能等數十秒，config 設 `120000` 寬裕些。
- ⚠ **真後端的行為（含錯誤路徑）要靠真後端驗**（離線 `file://` fixture 從不回錯）：C ABI 的後端錯誤護欄（`guard_backend_error`）就是真後端驗揪出來修的。
- ⚠ **reasoning 模型的 `max_tokens` 陷阱**（`google/gemma-4-e4b` 實測血淚）：思考鏈（`reasoning_content`）與答案（`content`）共用同一份 `max_tokens` 預算，設小值（如 600）會讓 reasoning 吃光、`content` 變空。**打 reasoning 時刻意不設 `--max-tokens`**，交後端用 context 上限。細節見 [gotchas/backend](workflows/common/gotchas/backend.md)。

## 更多文檔

| 想做／想查 | 去哪 |
|-----------|------|
| **用某語言呼叫 cllm**（C／C++／Lua／Fennel／s7／Python／Common Lisp）| [bindings/](bindings/README.md) |
| **裝成常駐 dev 環境**（`~/dev`，可 include/link、pkg-config cllm）| [install-dev.sh](install-dev.sh) |
| **一頁看懂**（資料流／型別對照／路由）| [docs/overview.html](docs/overview.html) |
| **API 參考**（C ABI／C++ 鏡像／CLI）| [docs/](docs/README.md) |
| **建置環境／vcpkg 依賴** | [docs/setup.md](docs/setup.md) |
| **平台分流／native HTTP／跨平台雙機** | [docs/platform.md](docs/platform.md) |
| **VSCode/gdb 除錯／clangd／設計要點** | [docs/debugging.md](docs/debugging.md) |
| **踩坑**（glaze／COMDAT／vcpkg／SAC／真後端）| [workflows/common/gotchas/](workflows/common/gotchas/README.md) |
| **跑測試／建置驗證** | [workflows/testing.md](workflows/testing.md) |
| **開發工作流／專案地圖** | [AGENTS.md](AGENTS.md)／[INDEX.md](INDEX.md) |
