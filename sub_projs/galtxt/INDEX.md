# INDEX — galtxt 專案地圖

整個專案的頂層導航。galtxt = **galgame 台詞生成的動手實驗場——把台詞產出鍛成「笨模型＋鷹架＋護欄」可靠管線，ai_core 北極星第一目標問題的落地執行處；依賴 [llm_forge/](../llm_forge/AGENTS.md)，跑通的機制才搬去固化**。AGENTS.md 只放主工作流 + 指向本檔；細節從這裡分流出去。

---

## Repo 佈局

工作流骨架 + 三個**玩具實驗場**（`try_1` / `try_2` / `try_3`，刻意不套框架、先跑通再往上抽）。

| 路徑 | 內容 |
|------|------|
| `AGENTS.md` / `CLAUDE.md` / `WORKFLOWS.md` / `INDEX.md` / `DEV-GUIDE.md` | 頂層路由 / 轉址 / 派發 / 地圖 / 結構整理參考 |
| `SESSION-LOG.md` / `WAIT_USER.md` | 活狀態（open-only）：進度 hub、待使用者項 |
| `workflows/` | 開發工作流（入口見 [WORKFLOWS.md](WORKFLOWS.md)）|
| [`try_1/`](try_1/README.md) | 玩具實驗場①：**s7 Scheme** 版 LLM 接口（`llm.scm`＋schema 生成簽章＋argv host `s7host.exe`）|
| [`try_2/`](try_2/README.md) | 玩具實驗場②：**C++ 內嵌 Lua 5.5** 版（`host.exe`＋`llm.lua`/`cli.lua`＋native `cjson.c` JSON codec；schema 生成 `--flag` CLI）|
| [`try_3/`](try_3/README.md) | 玩具實驗場③：**純 C++**（CMake + MinGW + VSCode/gdb；目前為可除錯骨架，不嵌腳本 VM）|

## 開發工作流

工作流的**選擇與入口**見 **[WORKFLOWS.md](WORKFLOWS.md)**——依「你想做什麼」派發。每個工作流的 durable 知識歸在 `workflows/<該工作流>/`（入口＝該夾 README 或主檔，含 `archive/` 封存過時文檔），具體流程在各自入口檔。

[DEV-GUIDE](DEV-GUIDE.md) 是**被動的結構整理參考**（結構整理原則 + 四級成長軌跡）——**只在要重構/整理結構時取用**。always-on 的**鐵律**在 [AGENTS.md](AGENTS.md)；碰原始碼的**程式碼慣例 + 導航 index 維護鏈**在 [common/conventions](workflows/common/conventions.md)。

## 通用（跨工作流共享）

| 路徑 | 內容 |
|------|------|
| [common/README](workflows/common/README.md) | 跨工作流共通：[gotchas](workflows/common/gotchas.md) 踩坑 + [conventions](workflows/common/conventions.md) 程式碼慣例 |

## 內容源頭與依賴

- **idea 礦脈（唯一內容來源）**：[../../workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md](../../workflows/notes/20260713-0956-galgame台詞生成-第一目標問題-llm_forge.md)。
- **依賴目標**：[llm_forge/](../llm_forge/AGENTS.md)（跑通的機制搬去固化）。**port 地基**：[../ver_1/try_implement/core_handy/](../ver_1/try_implement/core_handy/)。

## 活狀態（只列還沒完成的）

| 檔案 | 用途 |
|------|------|
| [SESSION-LOG](SESSION-LOG.md) | 進度 hub（repo 根）→ 各工作流 session-log（open-only）|
| [WAIT_USER](WAIT_USER.md) | 待**使用者**親自做/驗證的入口（repo 根；膨脹後拆 `wait_todo/` 分類檔）|
