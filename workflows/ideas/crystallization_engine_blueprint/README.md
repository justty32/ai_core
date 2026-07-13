---
title: 固化引擎 v0 工程藍圖——wake-sleep 庫學習 × 經濟淘汰 × 接地憲法
stage: spec-candidate  # 待與 workflows/spec/composite_spec.md 及 sub_projs/ver_1/try_implement/DECISIONS.md A4 合併審議，決定是否經 /spec 扶正
type: engineering-blueprint
date: 2026-06-27
status: 可實作藍圖（落成今夜的旗艦碰撞；非定案規範，規範權威仍在 workflows/spec/ 與 sub_projs/ver_1/try_implement/DECISIONS.md）
source_of_truth: |
  ai_core 北極星＝roadmap.md（§2 工廠/消費者、§3.4 治理、§3.5 飛輪、§3.6 固化引擎、§6.1 ATP v0、§7/§8 待決）；
  已凍結資料結構＝docs/kb/kb-ext/discussion_logs/round_2_synthesis.md（ATP v0 asset schema）；
  固化圓桌前案＝docs/kb/kb-ext/discussion_logs/crystallization_round_1_synthesis.md（ready() 四閘 + 三重籠子 + 三段 chain）；
  論文火花＝ideas/research/paper_collision_2026-06-27.md（彙整）＋三份 round2；論文真相層＝pas/paper_reading/summarize/*。
collides:
  - ai_core 固化引擎（飛輪：撤照/認證）
  - AI設計討論 bucket-brigade 經濟淘汰（破產/入庫）
  - 論文 wake-sleep 庫學習（DreamProver / Schank / SOAR）
cites: [[2604.26311]] [[2603.25975]] [[2507.14172]] [[2501.17848]] [[2606.24245]] [[2507.16405]] [[2512.06104]] [[2601.05280]] [[2606.26294]] [[2601.10904]]
note: 引用 [[arxiv_id]] 指回 pas/paper_reading/summarize/。本檔不改 index / session_log。
---

# 固化引擎 v0 工程藍圖

> **一句話**：把今夜反覆指向的旗艦碰撞——「ai_core 固化引擎 ＝ AI設計討論 bucket-brigade 經濟淘汰 ＝ 論文 wake-sleep 庫學習」——三者其實是**同一台機器的三種語言**。本檔把它落成一條可開工的離線管線：
> **wake（消費端就地收 ATP trace）→ sleep（離線貴智能：MDL 壓縮挑候選 + ILP 學判別謂詞 + e-graph 去重）→ 入庫（asset + grounding_class + 必要性閘）→ 經濟淘汰（命中率 / 描述長度 / LRU 破產）→ 消費端（便宜 matcher 高頻復用）。**

## 章節索引

本檔原為單一大檔，已按章節拆分為以下子檔（內容一字不刪，僅搬移＋連結修正）：

- [s1-3-goals-arch.md](./s1-3-goals-arch.md) ——
  ## 1 目標與非目標、## 2 三方術語對照表（ai_core 固化引擎 × bucket-brigade 經濟淘汰 × wake-sleep 庫學習）、## 3 架構（mermaid 管線圖：wake → sleep 五步 → 入庫閘 → 經濟淘汰）。
- [s4-schemas.md](./s4-schemas.md) ——
  ## 4 資料結構與介面草案：4.1 ATP asset schema 的固化增量（certificate 欄位）、4.2 wake span、4.3 固化決策函數簽章（python：emit_llm_span / scan / mdl_select / ilp_predicate / dedup / forge / ready / admit / evict）、4.4 固化引擎三段 chain 編排。
- [s5-7-grounding-milestones.md](./s5-7-grounding-milestones.md) ——
  ## 5 與崩潰定理 [[2601.05280]] 對齊（接地訊號來源、四道防污染防線、符號錨對齊）、## 6 分階段落地里程碑與風險（v0 / v0.5 / v1）、## 7 收束（本藍圖補了既有圓桌的哪一塊）。
