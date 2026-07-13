# INDEX — ai_core 專案地圖

整個專案的頂層導航。ai_core = **假設未來只剩便宜的本地小模型，做一套 CLI 框架把小笨模型包裝成可靠函式**（詳見 [roadmap.md](roadmap.md)）。AGENTS.md 只放路由 + 指向本檔；細節從這裡分流出去。本檔只描述「頂層」——某目錄內部複雜就進它自己的 README / INDEX。

## 專案狀態：三層可信度

此專案**已脫離純構想階段**：有正式核心 library、一整套設計規範、探索性原型與全綠測試。當前形態分三層（讀懂本 repo 的關鍵）：

| 層 | 位置 | 性質 | 可信度 |
|---|---|---|---|
| **北極星 / 戰略** | [roadmap.md](roadmap.md) | 為什麼做、終點長什麼樣、第一目標問題、v0 切片 | 主文件，判斷「該不該做、先做哪個」的尺 |
| **規範 (spec)** | [core_nature/](core_nature/) | 設計模式的精確定義（九軸、複合、CLI、library 契約） | **已扶正的內容是定案**；標「待填／待議」者未定 |
| **原型 (探索)** | [try_implement/](try_implement/) | 「先寫出來跑跑看」的遊樂場，暴露設計缺口 | **多數是提案**，非定案；扶正狀態見其 [DECISIONS.md](try_implement/DECISIONS.md) |

正式核心程式碼只有一處：`src/ai_core/_core.py`（＋ `tests/test_core.py`）。`try_implement/` 下的東西即使能跑，也**尚未**等於規範定案。

## Repo 佈局

| 路徑 | 內容 |
|------|------|
| [roadmap.md](roadmap.md) | ★ 北極星／戰略主文件（回來先讀這個）：初心、終點形狀、憑證准入治理、第一目標問題、v0 切片 |
| `src/ai_core/_core.py` | ★ 唯一的正式核心：`register()` / `intercept()` + 九軸驗證。契約摘要見 [common/conventions](workflows/common/conventions.md) |
| `tests/test_core.py` | 正式核心的測試 |
| [core_nature/](core_nature/) | 規範家族（spec）8 檔：overview / terminal_model / execution_forms / axis_spec / composite_spec / cli_spec / lib_spec / data_format |
| [try_implement/](try_implement/) | 原型遊樂場（多為提案）。入口 [README.md](try_implement/README.md)、懸案清單 [DECISIONS.md](try_implement/DECISIONS.md)；tools/ lib/ demos/ + 兩個 smoke_test |
| `core_handy/` | C++ 原型地基（server-form LLM daemon 等）。入口 [notes/00_index.md](core_handy/notes/00_index.md) |
| [ideas/](ideas/) | 點子捕捉／頭腦風暴軌產物：`raw/`（逐字）`cleaned/`（整理）`notes/`（匯總筆記）`brainstorm/`（critique/expand）|
| `docs/` | 架構文檔（ARCHITECTURE.md + architectures/）|
| `html/` | 多頁版總覽（index/architecture/status/vision/…）|
| `kb-ext/` `kb-ss/` `knowledge_base/` | 知識庫：擴展論述、原始碼／規範快照、圓桌 discussion_logs（各有 `00_INDEX`）|
| `funcs/` | 範例函式（`echo.sh` …）|
| `workflows/` | 開發工作流（入口見 [WORKFLOWS.md](WORKFLOWS.md)）|
| [archive/](archive/) | 歷史留存（已被取代、僅供追溯）：thinking/*.md、舊 overview.html、progress.md、other.md |

## 開發工作流

工作流的**選擇與入口**見 **[WORKFLOWS.md](WORKFLOWS.md)**——依「你想做什麼」派發。專屬 durable 知識歸在 `workflows/`（單檔或該夾 README），具體流程在各自入口檔。

[DEV-GUIDE](DEV-GUIDE.md) 是**被動的結構整理參考**（結構整理原則 + 四級成長軌跡）——只在要重構／整理結構時取用。always-on 的**鐵律**在 [AGENTS.md](AGENTS.md)；碰原始碼的**設計哲學 + 核心契約 + 程式碼慣例**在 [common/conventions](workflows/common/conventions.md)。

## 通用（跨工作流共享）

| 路徑 | 內容 |
|------|------|
| [common/README](workflows/common/README.md) | 跨工作流共通：[conventions](workflows/common/conventions.md)（設計哲學 + 核心契約 + 程式碼慣例）+ [gotchas](workflows/common/gotchas.md)（踩坑）|

## 活狀態（只列還沒完成的）

| 檔案 | 用途 |
|------|------|
| [SESSION-LOG](SESSION-LOG.md) | 進度 hub（repo 根，含固定閱讀順序）（open-only）|
| [WAIT_USER](WAIT_USER.md) | 待**使用者**親自做/驗證、追認的入口（repo 根）|
