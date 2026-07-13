# intake — 口述線一條龍（工作流）

← [WORKFLOWS](../../WORKFLOWS.md)｜[INDEX](../../INDEX.md)

**口述輸入友善的點子捕捉軌**：把一段口述從原文逐字一路加工到匯總筆記。對應 slash command `/intake`。與 [idea-capture](../idea-capture.md)（頭腦風暴 critique/expand）並列，同屬點子軌但職責分開。

## 三階段與產物落點

| 階段 | 這是什麼 | 落點 |
|------|---------|------|
| **WF1 原文逐字** | 口述原文一字不改 | [`raw/`](raw/)（本夾）|
| **WF2 初步整理** | 逐字稿去雜訊、斷句 | [`cleaned/`](cleaned/)（本夾）|
| **WF3 匯總筆記** | 方向定調、長談記錄的成熟筆記 | [`../notes/`](../notes/)（已獨立成 workflows/notes/）|

`raw/`、`cleaned/` 是本工作流的中間產物、隨工作流放這裡；成熟的匯總筆記落到獨立的 [workflows/notes/](../notes/)。

## ai_core 特化：dogfood 自己的 LLM 基礎設施

`/intake` 的 LLM 加工**不派 Claude Code agent，改 shell out 給 [`idea.py`](../../sub_projs/ver_1/try_implement/tools/idea.py) 打真 API**（dogfood ai_core 自己的 LLM 基礎設施）。

- **保留「主 agent 即時回應」模式**（語音輸入友善），但動作丟的是**背景 Bash 呼叫 `idea ingest`**（非背景 subagent）。
- `idea.py` 的 `ingest` 子命令＝口述一條龍；預設經**元件 1 entry manager** 路由，串 `bind`（元件 2）→ entry manager → 真 backend → API。LLM 子命令宣告第九軸 `nondeterministic:true`。這是 roadmap「廉價小模型消費者」的第一個真實串接。

## 接真 LLM 的環境變數

要接真 LLM 需設 `AI_CORE_LLM_PROVIDER` / `AI_CORE_LLM_BASE_URL` / `AI_CORE_LLM_MODEL`；**未設則 EchoBackend 回顯**（測試用）。

## 權威來源

指令細則自含於 [`.claude/commands/intake.md`](../../.claude/commands/intake.md)；idea 工具本身見 [try_implement/README.md](../../sub_projs/ver_1/try_implement/README.md)。
