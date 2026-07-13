# idea-capture — 點子捕捉／頭腦風暴軌（單檔工作流）

← [WORKFLOWS](../WORKFLOWS.md)｜[INDEX](../INDEX.md)

從 TTemp 借來的**點子捕捉／頭腦風暴軌**，與本 repo 的程式碼／規範工作並行、互不干擾。三個 slash command 對應三個階段，產物統一落在 [ideas/](ideas/)（同在 `workflows/` 下）。

## 三個指令

| 指令 | 用途 | 產物落點 |
|------|------|---------|
| `/intake` | 口述線一條龍：原文逐字 → 初步整理 → 匯總筆記 | `ideas/`（`raw/` 逐字、`cleaned/` 整理、`notes/` 匯總筆記）|
| `/critique` | 頭腦風暴・找漏洞（指出哪裡有錯／考慮不周）| `ideas/brainstorm/` |
| `/expand` | 頭腦風暴・擴展靈感（把點子變大、變多）| `ideas/brainstorm/` |

## ai_core 特化（2026-06-08）：dogfood 自己的 LLM 基礎設施

這三個指令的 LLM 加工**不派 Claude Code agent，改 shell out 給 [`try_implement/tools/idea.py`](../sub_projs/ver_1/try_implement/tools/idea.py) 打真 API**（dogfood ai_core 自己的 LLM 基礎設施）。

- **只有 `/intake`（語音輸入）保留「主 agent 即時回應」模式**，但動作丟的是**背景 Bash 呼叫 `idea ingest`**（非背景 subagent）。
- **`/critique`、`/expand` 同步 shell out** 給 `idea critique` / `idea expand`。
- `idea.py` 子命令：`clean` / `notes` / `critique` / `expand`（純 filter）＋ `ingest`（口述一條龍）。預設經**元件 1 entry manager** 路由，串起 `bind`（元件 2）→ entry manager（元件 1）→ 真 backend → API。每個 LLM 子命令宣告第九軸 `nondeterministic:true`。這是 roadmap「廉價小模型消費者」的第一個真實串接。

## 接真 LLM 的環境變數

要接真 LLM 需設 `AI_CORE_LLM_PROVIDER` / `AI_CORE_LLM_BASE_URL` / `AI_CORE_LLM_MODEL` 環境變數；**未設則 EchoBackend 回顯**（測試用）。

## 權威來源

各 slash command 的細則自含於 `.claude/commands/{intake,critique,expand}.md`；idea 工具本身見 [try_implement/README.md](../sub_projs/ver_1/try_implement/README.md)。
