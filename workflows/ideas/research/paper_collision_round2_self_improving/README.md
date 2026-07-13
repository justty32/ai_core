---
title: 論文碰撞・第二輪——自我改進 agent × ai_core 智能分層
stage: research-idea
type: collision-sparks
date: 2026-06-27
source_of_truth: ~/repo/pas/paper_reading/summarize/（各篇摘要） + deep/self_improving_program_synthesis.md
ai_core_refs: roadmap.md（§2 工廠/消費者、§3 治理、§5 兩層、§6.1 ATP v0、§7 待決規範、§8 開放問題）
covers:
  - 2601.05280  # 自我改進的極限（崩潰定理）
  - 2606.26294  # 紅皇后哥德爾機（協同演化評估者）
  - 2606.26327  # EVOM（雙層元演化）
  - 2603.28416  # LLM 演化發現 RL 演算法
  - 2512.16406  # 自我參照圖超網路（可演化性）
  - 2505.22954  # Darwin-Gödel Machine
  - 2406.18532  # 代理符號學習
  - 2101.03958  # 演化 RL 演算法
  - 2507.17668  # 如何元學習 RL 演算法
  - 2503.16048  # 元學習灌入的是神經機制非貝氏先驗
---

# 論文碰撞・第二輪：自我改進 agent × ai_core 智能分層

> 真相層：`paper_reading/summarize/` 各篇摘要與 `deep/self_improving_program_synthesis.md`。本文只做「論文技術 → ai_core 具體設計決定」的碰撞，引用一律以 [[arxiv_id]] 指回。
> ai_core 北極星見 `roadmap.md`。本文不改 index / session_log。

## 章節索引

- **[§0 + 火花 1-3](map-sparks-1-3.md)**：一頁地圖、接地憲法（火花 1）、EVOM 雙層元演化（火花 2）、紅皇后活證書（火花 3）
- **[火花 4-5](sparks-4-5.md)**：verifier 分級路由（火花 4）、機制 > 相關（火花 5）
- **[火花 6-7 + 收束](sparks-6-7-closing.md)**：符號錨背書（火花 6）、自主調節 regime（火花 7）、潛力排序與下一步
