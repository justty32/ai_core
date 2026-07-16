# INDEX — cllm 專案地圖

整個專案的頂層導航。cllm = **對外 C ABI 共享庫 `libcllm.so`（唯一入口 `llm_ask`）＋建在其上的 `llm` unix filter CLI；純 C++23、CMake+Ninja+vcpkg+glaze**。AGENTS.md 只放路由＋指向本檔；技術全貌在 [README.md](README.md)，細節從這裡分流出去。

---

## Repo 佈局

| 路徑 | 內容 |
|------|------|
| [`README.md`](README.md) | **技術入口／完整使用文檔**：建置、C ABI（`llm_ask` 統一入口）、`llm` CLI、vcpkg/glaze 依賴、native HTTP、接真後端、跨平台、VSCode/nvim 除錯 |
| `src/` | 現行原始碼：`http.{hpp,cpp}`（native HTTP 傳輸）＋`cabi.h`＋`cabi_{client,request,response,context}.h`（對外 C ABI 傘檔＋功能頭）＋`cabi.cpp`／`cabi_request.cpp`／`cabi_response.cpp`／`cabi_stream.cpp`（C ABI 實作，按關注點拆檔）＋`cabi_internal.hpp`（實作共用內部頭）＋`cabi.hpp`＋`cabi_{context,request,response}.hpp`（C++ 薄鏡像 `llm::abi`）＋`cli.{hpp,cpp}`＋`main.cpp`（`llm` CLI）|
| `archived/` | 舊 L0 封存（`git mv` 保史、內容一字未改）：`llm.{hpp,cpp}`／三擴充（`llm_tool`／`llm_media`／`llm_json`）／`llm_schema.hpp`／舊反射 `cli`／`demo`／舊 `main`。重構時融進 C ABI 實作、不再對外提供獨立函數 |
| `test/` | `cli_smoke.sh`（離線黑箱煙霧測試，17/17）＋`fixtures/{fake,fake_stream,fake_tool,fake_json}/`（版控的假回應，`file://` 餵進 CLI）|
| `CMakeLists.txt` / `CMakePresets.json` / `vcpkg.json` | 建置：兩交付物 target（`cllm` SHARED＝`libcllm.so`、`llm` executable）＋兩組 preset（`mingw-*`／`linux-*`）＋vcpkg manifest（glaze）|
| `.clangd` / `.vscode/` | 編輯器整合（clangd 讀 `build/compile_commands.json`；`.vscode/` 為 Windows VSCode 專用）|
| `build/` | CMake 產出（**gitignored**：`libcllm.so`／`llm`／`compile_commands.json`／`vcpkg_installed/`）|
| `workflows/` | 開發工作流（入口見 [WORKFLOWS.md](WORKFLOWS.md)）|

## 開發工作流

工作流的**選擇與入口**見 **[WORKFLOWS.md](WORKFLOWS.md)**——依「你想做什麼」派發。每個工作流的 durable 知識歸在 `workflows/<該工作流>/`（入口＝該夾 README 或主檔，含 `archive/` 封存過時文檔）。

[DEV-GUIDE](DEV-GUIDE.md) 是**被動的結構整理參考**——**只在要重構／整理結構時取用**。always-on 的**鐵律**在 [AGENTS.md](AGENTS.md)；碰原始碼的**程式碼慣例 + 導航 index 維護鏈**在 [common/conventions](workflows/common/conventions.md)。

## 通用（跨工作流共享）

| 路徑 | 內容 |
|------|------|
| [common/README](workflows/common/README.md) | 跨工作流共通：[CODE_MAP](workflows/common/code-map/CODE_MAP.md) 原始碼導航（修改前先讀）＋ [conventions](workflows/common/conventions.md) 程式碼慣例 ＋ [gotchas](workflows/common/gotchas.md) 踩坑（glaze／匿名 namespace COMDAT／vcpkg／clangd／SAC／真後端）|

## 活狀態（只列還沒完成的）

| 檔案 | 用途 |
|------|------|
| [SESSION-LOG](SESSION-LOG.md) | 進度 hub（open-only）|
| [WAIT_USER](WAIT_USER.md) | 待**使用者**親自做／驗證的入口 |
