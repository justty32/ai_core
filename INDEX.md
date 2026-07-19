# INDEX — ai_core 專案地圖

整個專案的頂層導航。ai_core = **假設未來只剩便宜的本地小模型，做一套 CLI 框架把小笨模型包裝成可靠函式**（詳見 [roadmap.md](workflows/roadmap/README.md)）。AGENTS.md 只放路由 + 指向本檔；細節從這裡分流出去。本檔只描述「頂層」——某目錄內部複雜就進它自己的 README / INDEX。

## 專案狀態：可信度分層

**當前主線是規劃層**（`workflows/` 的 roadmap＋spec＋ideas＋plans）。讀懂本 repo 的關鍵在分辨哪層是定案、哪層是提案：

| 層 | 位置 | 性質 | 可信度 |
|---|---|---|---|
| **北極星 / 戰略** | [roadmap.md](workflows/roadmap/README.md) | 為什麼做、終點長什麼樣、第一目標問題、v0 切片 | 主文件，判斷「該不該做、先做哪個」的尺 |
| **規範 (spec)** | [workflows/spec/](workflows/spec/) | 設計模式的精確定義（九軸、複合、CLI、library 契約） | **已扶正的內容是定案**；標「待填／待議」者未定 |
| **提案 / 規劃** | [ideas/](workflows/ideas/README.md)、[plans/](workflows/plans/README.md) | 點子、研究、spec 候選厚檔、施工規劃 | **多為提案／彙整層**，非定案；沿管線成熟後才往 spec 走 |

（早期的 Python 實作已封存於 `sub_projs/`，半封存狀態；當前無在跑的實作。）

## Repo 佈局

| 路徑 | 內容 |
|------|------|
| [workflows/](WORKFLOWS.md) | ★ 當前主線：開發工作流＋活的規劃內容。[roadmap.md](workflows/roadmap/README.md)（★ 北極星／戰略主文件，回來先讀這個）、[spec/](workflows/spec/)（權威設計規範 8 項＝原 core_nature，扶正者為定案；其中 lib_spec／axis_spec／execution_forms／composite_spec 已超標拆成資料夾＋index）、[plans/](workflows/plans/README.md)（spec 的實作規劃）、[intake/](workflows/intake/README.md)（口述線一條龍，產 raw/cleaned）、[notes/](workflows/notes/)（匯總筆記）、[ideas/](workflows/ideas/README.md)（腦暴 brainstorm＋研究 research＋spec 候選）、idea-capture.md（腦暴 critique/expand）、[investigation/](workflows/investigation/README.md)（技術/可行性調查，如 linux-file-metadata）、common/。派發入口 [WORKFLOWS.md](WORKFLOWS.md) |
| [docs/](docs/README.md) | 快照家族入口（spec 已移出）：`kb/`（知識庫快照三夾 knowledge_base・kb-ext・kb-ss，凍結快照）、`html/`（手工多頁總覽站，vibe check）。入口 [docs/README.md](docs/README.md) |
| [sub_projs/](sub_projs/README.md) | 次級物：`llm_forge/`（框架規劃地／爐子：固化階梯等機制之家）＋ `galtxt/`（動手實驗場／第一把刀：依賴 llm_forge，先跑通再搬回固化）＋ `cllm/`（獨立 C LLM 產物：對外 C ABI `libcllm.so`＋`llm` unix filter CLI，源自 galtxt/try_3 抽離）＋ `comfy/`（舒適 Common Lisp 地基＋VSCode/Alive debug 環境，框架外實驗場）＋ `handy/`（路徑一試驗田／OS 當 AI agent：靈活小腳本集，已有 llme dispatcher＋zhtw 薄包裝、規劃 wf/daemon，重度用 cllm；資料夾採分層工作流形式）＋ `unipath/`（歸一於路徑基材：Plan 9＋9P over FUSE，活 process 執行態暴露成路徑樹，step 1–6 全綠）＋ `cllm-apps/`（handy 四工具多語言移植：python/cpp/lisp-handy，shell-out 對齊 Fennel 原型）＋ `cllm-lab/`（cllm 綁定十語言遊樂場）＋ `ver_1/`（早期 Python 實作封存，半封存、不在現役維護鏈）。入口 [sub_projs/README.md](sub_projs/README.md) |

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
| [SESSION-LOG](SESSION-LOG.md) | 進度 hub（repo 根）（open-only）|
| [WAIT_USER](WAIT_USER.md) | 待**使用者**親自做/驗證、追認的入口（repo 根）|
