# 01 · ATP v0（程式碼填碼切片）

> 定案＝roadmap §6 v0 的具體規格。**已登記進 roadmap §6.1 + DECISIONS.md**。過程：`discussion_logs/round_2_*` + `round_3_ratification.md`。

## 一句話

V0＝讓一個 **append-only 的 asset JSON** 從 `idea.py`（工廠）→ `hub`（轉運站+trace 接力）→ `sfc.py`（消費），途中被裁剪 / 定位 / 蓋最弱證書章 / 環境變數追蹤。任務＝在錨點 `# AI_CORE:INSERT_HERE` 插入一個函式，成功用確定性驗證（ast.parse 過 + 目標節點存在 + 簽名符合）。

## 六要素

| 要素 | 定案 |
|------|------|
| 資料結構 | append-only asset JSON，逐站追加欄位、**不改寫上游**（P1）|
| 裁剪 | `skeletonize()` 純 ast，保簽名/錨點鄰域、刪實作；`pruning.strategy ∈ {raw, ast_skeleton}` |
| 隔離 | `subprocess(env=白名單)` 軟隔離，V0 不上 bubblewrap |
| 溯源 | 一個 `AI_CORE_TRACE_ID` 環境變數 + asset 內 `trace[]`，落盤 NDJSON（不上 sqlite）|
| 治理 | 證書是 asset 的 `certificate` 欄（寄生第九軸），標註不攔截；`status ∈ {uncertified, syntax_ok, rejected}`，`rejected` 帶 `reason ∈ {guardrail_violation, retry_exhausted}`（P2）；AST 裁剪三層護欄 fail-closed |
| 評測 | `TrimTrace` + tier×method pivot（raw vs ast_skeleton）；配對主鍵 `task_id`、每格 N≥30（P3）|

## 三條凍結補丁

- **P1**（append-only 條文化）：頂層除識別欄外不可後寫，站間輸出走 `trace[]` append。
- **P2**（`rejected.reason`）：分 `guardrail_violation`（帶毒、永久終態）vs `retry_exhausted`（能力不足）。
- **P3**（`task_id`）：同一任務在 raw / ast_skeleton 兩臂共用，供 benchmark 配對比較。

## 待建檔（開工次序）

`asset.py → trace.py → skeleton.py → isolation.py → line_assistant.py → demos/v0_pipeline.py`，全純標準庫。
