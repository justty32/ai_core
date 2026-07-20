# cllm docs — API 參考

← [技術入口 README](../README.md)｜[專案地圖 INDEX](../INDEX.md)

`libcllm.so` / `llm` CLI 的**正式 API 參考**。README 是敘事型入口（建置／接真後端／跨平台／除錯）；這裡是**逐型別／逐欄位／逐旗標**的 reference，給要查「這個 struct 有哪些欄位、這個旗標吃什麼」的人。

> 每份 .md 守 **≤ 8192 bytes**（本專案文檔拆檔標準）；查得快、改得動。

## 先看哪份

| 你是… | 讀這份 |
|-------|--------|
| **想一眼看懂整體資料流**（`llm_ask` 一個入口、輸入→輸出、三層交付物）| **[`overview.html`](overview.html)** — 視覺總覽，減輕認知負擔 |
| **寫純 C，連 `libcllm.so`** | [C ABI 參考](c-abi-reference.md)（總覽＋`llm_ask`）→ [輸入型](c-abi-input.md)／[輸出型與控制](c-abi-output.md) |
| **寫 C++，用 `llm::abi`** | [C++ 鏡像參考](cpp-mirror-reference.md) |
| **不寫程式碼，用 `llm` 指令** | [CLI 手冊](cli-manual.md) |
| **要建置／裝依賴** | [建置環境與依賴](setup.md) |
| **查平台分流／跨機 preset** | [平台與傳輸](platform.md) |
| **除錯／編輯器 LSP** | [除錯與編輯器整合](debugging.md) |
| **要對某後端做結構化輸出**（json_schema／tool 哪個吃）| [各後端結構化輸出矩陣](backend-structured-output.md) |

## 檔案清單

| 檔 | 內容 |
|----|------|
| [`overview.html`](overview.html) | 視覺總覽：三層交付物、`llm_ask` 資料流、phase 生命週期、型別對照、「我想做…」路由 |
| [`c-abi-reference.md`](c-abi-reference.md) | C ABI 總覽＋傘檔佈局＋記憶體約定＋統一入口 `llm_ask`／`llm_status_t` |
| [`c-abi-input.md`](c-abi-input.md) | C ABI 輸入型：`llm_client_t`／`field_mask`／`llm_request_t`／schema／tool_def／media_in／modality |
| [`c-abi-output.md`](c-abi-output.md) | C ABI 輸出型與控制：四 handler／tool_call／media_out／`llm_context_t`／`llm_phase_t`／cancel |
| [`cpp-mirror-reference.md`](cpp-mirror-reference.md) | C++ 薄鏡像 `llm::abi`：`Client`／`Request`／`Handlers`／`Context`＋型別對照＋範例 |
| [`cli-manual.md`](cli-manual.md) | `llm` CLI：旗標（固定＋反射生成）／config 三層來源／退出碼／接真後端／離線自測 |
| [`setup.md`](setup.md) | 建置環境需求（Windows／Linux）＋ vcpkg manifest 依賴（glaze／triplet）|
| [`platform.md`](platform.md) | 平台分流：native HTTP（WinHTTP／libcurl／`file://`）＋ 跨平台雙機 preset |
| [`debugging.md`](debugging.md) | VSCode/gdb 除錯 ＋ clangd（nvim/VSCode 共用）＋ 設計要點（static／中文／SAC／gdb）|
| [`backend-structured-output.md`](backend-structured-output.md) | 主題：15 家後端對 json_schema／json_object／tool calling 的支援矩陣（DeepSeek 實測＋其餘文檔依據，2026-07-20）|

## 這些 docs 怎麼維護

API 參考的**真相源是頭檔**（`src/cabi*.h`／`cabi*.hpp`）與 `src/cli.cpp`。改動這些 struct／旗標時，順手更新對應的 .md（尤其反射生成的 CLI 旗標表，以 `llm --help` 為準）。敘事型知識（建置／踩坑）仍歸 [README](../README.md)／[common/gotchas](../workflows/common/gotchas/README.md)，不搬進這裡。
