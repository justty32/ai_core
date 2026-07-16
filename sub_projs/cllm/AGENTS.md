# cllm — AI agent 專案備忘

cllm = **把 LLM 收成一支對外 C ABI 共享庫 `libcllm.so`（`extern "C"`，唯一入口 `llm_ask`）＋建在其上的 `llm` unix filter CLI**。純 C++23、零腳本 VM，建置走 CMake + Ninja + vcpkg + glaze。源自 galtxt 的純 C++ 實驗線（原 `galtxt/try_3`），收斂成兩交付物後抽離成獨立產物。本檔是**最頂層路由器**：只指向下一層，**durable 細節一律不寫這裡**。

## 先讀哪裡

- **想上手／看技術全貌** → **[README.md](README.md)**：完整使用文檔（建置、C ABI、CLI、vcpkg、跨平台、接真後端、除錯）。
- **使用者要你動手做某件事** → **[WORKFLOWS.md](WORKFLOWS.md)**：依使用者意圖派發到對應工作流，再讀該工作流入口。
- **想看專案長怎樣** → **[INDEX.md](INDEX.md)**：repo 頂層結構地圖。

## 分層思想（本專案的組織原則）

整個 repo 是一棵**分層樹**，每一層**只指向下一層、不存下層的細節**：

```
AGENTS.md（本檔，最頂）→ README.md（技術入口）/ WORKFLOWS.md / INDEX.md → 各工作流入口 → 工作流內容…
```

- **README**＝初入一個資料夾**先讀的入口／導引**；**INDEX**＝**描述該資料夾頂層結構**的索引。
- **durable 知識歸到它所屬的那一層／那個工作流**，絕不往上堆——所以 AGENTS.md 才這麼薄。要某主題的細節，順著上面的樹往下走，不在本檔找。
- **[DEV-GUIDE.md](DEV-GUIDE.md) 是被動參考**（結構整理原則 + 四級成長軌跡）——**只在你要重構／整理結構時才取用**。只在**碰原始碼**時適用的**程式碼慣例 + 導航 index 維護鏈**在 [common/conventions](workflows/common/conventions.md)。

## 鐵律（always-on，任何工作流任何時候都遵守）

1. **改動程式碼後跑驗證確認行為不變**：快速驗證＝`cmake --build --preset linux-debug && bash test/cli_smoke.sh`（離線黑箱，17/17 綠）。
2. **未經確認不 push、不開新工作**（commit 到主分支是慣例，push 先確認）。
3. **對外 C ABI（`src/cabi.h`）是穩定介面**：改扁平結構／`llm_ask` 簽章會震到所有 C 客戶端與 `llm` CLI——動之前先想相容性。
4. **回覆、註解、留檔用繁體中文**；程式碼識別子、shell 指令、技術名詞保留原文。
5. 各工作流的**具體流程在它自己的入口檔**，不在頂層。

## 開發環境

**跨機開發**：Windows（MinGW-w64 g++16＋`C:/dev/vcpkg`）與 Linux（Manjaro，系統 g++16＋`~/vcpkg`）雙機皆已實測全綠。各機路徑釘在 `CMakePresets.json`（`mingw-*` / `linux-*` 兩組 preset，各掛 `condition` 判平台）。完整環境需求、跨平台細節、換機改哪幾處，全在 [README.md](README.md)「需要的東西」「跨平台」兩節。

## 主工作流（進度與待測）

事情告一段落、因應需求結束、或臨時中止時 → 把**還沒完成**的活狀態記到進度；需要**使用者親自做／驗證**的（實機環境、外部工具實跑、需權限）→ 記到待使用者。兩者都**只列 open**，完成即移除、不留已完成清單。

- **進度** → [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者** → [WAIT_USER.md](WAIT_USER.md)
