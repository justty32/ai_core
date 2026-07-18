# INDEX — handy 專案地圖

整個 handy 的頂層導航。handy = **路徑一（OS 當 AI agent）的試驗田：一組小腳本/小程式，拿現成程式（cllm）用腳本包裝、資料夾＝callable、按慣例組合**。AGENTS.md 只放路由＋鐵律＋活狀態指標；細節從這裡分流出去。

---

## 定位：一塊試驗田，陸續往裡丟小程式

**handy 本身就是那塊試驗田**——一個資料夾，之後往裡放**一大堆小程式／小腳本**（＝住戶）。不預先分格、不搞子田；每個小東西各自成一攤，用**同一套方法論**：拿現成程式、腳本包裝、**資料夾＝callable**、按慣例組合。這是 [0708「一切皆檔案」note](../../workflows/notes/20260708-1040-一切皆檔案-list-統一描述子-複合路徑.md)「folder-as-callable」落地的第一個真身。

## Repo 佈局

| 路徑 | 內容 |
|------|------|
| `llme/` | 住戶①：cllm 之上的**多 endpoint dispatcher**（資料夾＝callable）。入口 [llme/README.md](llme/README.md)。內：`_exec`（shell shim 入口）→ `_exec.fnl`（Fennel 本體）＋ `configs/<endpoint>.json`；外殼 `llme.sh` 轉發。 |
| `zhtw` | 住戶②：**薄包裝範例**（單檔 Fennel）——固定取樣參數＋system＋前置，轉呼 `llme deepseek`（繁中翻譯人格）。入口＝檔頭註解。 |
| `workflows/` | 工作流（入口見 [WORKFLOWS.md](WORKFLOWS.md)）：`new-resident.md`、`dev-env.md`、定期 `tick`/`routines`/`schedule`、`common/`（`gotchas.md`＋`conventions.md` code map）。 |
| `.claude/commands/` | slash 指令（[`/wf-tick`](.claude/commands/wf-tick.md) 驅動定期心跳）|

> 某住戶內部複雜就在它自己的資料夾放 README/INDEX，本檔只留一句話＋連結——本檔永遠只描述「頂層」。

## 住戶（現有＋規劃）

| 住戶 | 狀態 | 一句話 | 入口 |
|------|------|--------|------|
| **llme** | 可用・已驗真後端 | 換 endpoint 名＝換模型；auto-inject api key | [llme/README.md](llme/README.md) |
| **zhtw** | 可用 | 繁中翻譯薄包裝；stdin 管線公民 | 檔頭註解 |
| **wf** | 可用（MVP）| **兩層任務派發器**：llme(DeepSeek) 當路由腦判「要不要動手」→ 只需腦走 `llme`、要動手走 `claude -p`。`-b`/`-a` 強制。 | `wf` 檔頭註解 |
| **daemon** | 待動手 | 常駐小程式，client 寫命令進檔/socket → `claude -p` headless run | 見 [dev-env](workflows/dev-env.md)「daemon 觸發條件」＋ [SESSION-LOG](SESSION-LOG.md) |

## 不重造（本專案吃現成、別憑記憶重建）

| 要的 | 現成實體 | 位置 |
|---|---|---|
| daemon 骨架（常駐＋socket＋佇列＋單例計量） | `llm_entry.cpp` 的 `--serve <sock>` | `../ver_1/try_implement/core_handy/examples/llm_entry.cpp` |
| 登入／key 失效重登／patch config | `llm-login` | `../cllm/tools/llm-login/` |
| 命令 log／memoize／可靠度原料 | NDJSON trace 構想 | note 0718 §6.3 |
| cllm 現成件：`libcllm.so`（唯一入口 `llm_ask`）＋`llm` CLI、`tools/llm-login`、`tools/anthropic-proxy` | — | [cllm/AGENTS.md](../cllm/AGENTS.md) |

## 設計脈絡（來龍去脈／設計決定）

- [設計 note：daemon 與 llme 兩塊試驗田](../../workflows/notes/20260718-0924-路徑一-daemon與llme兩塊試驗田.md)（本專案收斂記錄）
- 接同日 [三條路岔路盤點](../../workflows/notes/20260718-0727-三條路岔路盤點-os語言-cllm函數-lisp蒸餾.md)

## 工作流

工作流的**選擇與入口**見 **[WORKFLOWS.md](WORKFLOWS.md)** 的派發表。每個工作流的 durable 知識歸在 `workflows/<該工作流>.md`（或資料夾＋`archive/`），具體流程在各自入口檔。[DEV-GUIDE](DEV-GUIDE.md) 是**被動的結構整理參考**——只在要重構/整理結構時取用。always-on 的**鐵律**在 [AGENTS.md](AGENTS.md)。

## 通用（跨工作流共享）

| 路徑 | 內容 |
|------|------|
| [workflows/common/README](workflows/common/README.md) | 跨工作流共通入口 |
| [workflows/common/gotchas.md](workflows/common/gotchas.md) | 跨住戶踩坑（glaze 嚴格 config、DeepSeek `--schema`、reasoning max-tokens…）|
| [workflows/common/conventions.md](workflows/common/conventions.md) | **程式碼慣例 + code map**（哪個檔負責什麼；碰程式碼前先讀）|
| [workflows/dev-env.md](workflows/dev-env.md) | 開發環境：技術棧（LuaJIT/Fennel）、cllm 依賴、後端、build、daemon 觸發條件、驗證 |
| [workflows/new-resident.md](workflows/new-resident.md) | 加新住戶的方法論＋通用慣例（照抄地基）|

## 活狀態（只列還沒完成的）

| 檔案 | 用途 |
|------|------|
| [SESSION-LOG](SESSION-LOG.md) | 進度 hub（handy 根）open-only |
| [WAIT_USER](WAIT_USER.md) | 待**使用者**親自做/驗證的入口 open-only |
