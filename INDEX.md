# INDEX — ai_core 專案地圖

整個專案的頂層導航。ai_core = **假設未來只剩便宜的本地小模型，做一套 CLI 框架把小笨模型包裝成可靠函式**（詳見 [roadmap.md](workflows/roadmap.md)）。AGENTS.md 只放路由 + 指向本檔；細節從這裡分流出去。本檔只描述「頂層」——某目錄內部複雜就進它自己的 README / INDEX。

## 專案狀態：三層可信度

repo 剛重新分層：**當前主線是規劃層**（`workflows/` 的 roadmap＋ideas＋spec），而最早那套 Python 實作已整套**降級為封存參考**（`sub_projs/ver_1/`）。當前形態分三層（讀懂本 repo 的關鍵）：

| 層 | 位置 | 性質 | 可信度 |
|---|---|---|---|
| **北極星 / 戰略** | [roadmap.md](workflows/roadmap.md) | 為什麼做、終點長什麼樣、第一目標問題、v0 切片 | 主文件，判斷「該不該做、先做哪個」的尺 |
| **規範 (spec)** | [workflows/spec/](workflows/spec/) | 設計模式的精確定義（九軸、複合、CLI、library 契約） | **已扶正的內容是定案**；標「待填／待議」者未定 |
| **封存實作 (ver_1)** | [sub_projs/ver_1/](sub_projs/README.md) | 最早的 Python 實作版：`src/` 核心＋`tests/`＋`try_implement/`（含 core_handy）＋`examples/` | **僅供參考、基本等同封存**；不再是當前主線、不精整 |

當前主線在規劃層（roadmap＋ideas＋spec）；`sub_projs/ver_1/` 下的實作即使能跑，也只當歷史參考，**不再**是 repo 主線。

## Repo 佈局

| 路徑 | 內容 |
|------|------|
| [workflows/](WORKFLOWS.md) | ★ 當前主線：開發工作流＋活的規劃內容。[roadmap.md](workflows/roadmap.md)（★ 北極星／戰略主文件，回來先讀這個）、[ideas/](workflows/ideas/README.md)（點子與研究的家，含規劃管線落點）、[spec/](workflows/spec/)（權威設計規範 8 檔＝原 core_nature，扶正者為定案）、testing.md、idea-capture.md、common/。派發入口 [WORKFLOWS.md](WORKFLOWS.md) |
| [docs/](docs/README.md) | 快照家族入口（spec 已移出）：`kb/`（知識庫快照三夾 knowledge_base・kb-ext・kb-ss，凍結快照）、`html/`（手工多頁總覽站，vibe check）。入口 [docs/README.md](docs/README.md) |
| [sub_projs/](sub_projs/README.md) | 半獨立子專案：`ver_1/`（★ 封存的最早 Python 實作版＝src＋tests＋try_implement〔含 core_handy〕＋examples＋pyproject，僅供參考、不精整）、`llm_forge/`（galgame 台詞生成器規劃，第一目標問題）。入口 [sub_projs/README.md](sub_projs/README.md) |
| [archive/](archive/README.md) | 歷史留存（已被取代、僅供追溯）：thinking/*.md、舊 overview.html、progress.md、other.md、architecture_docs/（舊架構文檔）。入口 [archive/README.md](archive/README.md) |

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
