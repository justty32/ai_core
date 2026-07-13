# intake — 口述線一條龍（工作流）

← [WORKFLOWS](../../WORKFLOWS.md)｜[INDEX](../../INDEX.md)

**口述輸入友善的點子捕捉軌**：把一段口述從原文逐字一路加工到匯總筆記。與 [idea-capture](../idea-capture.md)（頭腦風暴 critique/expand）並列，同屬點子軌但職責分開。

## 三階段與產物落點

| 階段 | 這是什麼 | 落點 |
|------|---------|------|
| **WF1 原文逐字** | 口述原文一字不改 | [`raw/`](raw/)（本夾）|
| **WF2 初步整理** | 逐字稿去雜訊、斷句 | [`cleaned/`](cleaned/)（本夾）|
| **WF3 匯總筆記** | 方向定調、長談記錄的成熟筆記 | [`../notes/`](../notes/)（已獨立成 workflows/notes/）|

`raw/`、`cleaned/` 是本工作流的中間產物、隨工作流放這裡；成熟的匯總筆記落到獨立的 [workflows/notes/](../notes/)。

## 備註

原本 WF1→3 的 LLM 加工是 shell out 給 ai_core 自己的 `idea.py`（`ingest` 子命令＝口述一條龍，dogfood 自家 LLM 基礎設施：經元件 1 entry manager 路由、宣告第九軸 `nondeterministic:true`，未設 `AI_CORE_LLM_*` 環境變數則 EchoBackend 回顯）。該工具已隨 Python 實作封存進 [ver_1](../../sub_projs/ver_1/try_implement/README.md)——當前無在跑的實作，要用把它當參考。
