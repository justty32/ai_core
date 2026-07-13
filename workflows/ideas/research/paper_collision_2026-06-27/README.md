---
title: 論文碰撞・正式彙整版——paper_reading 論文群 × ai_core 北極星
stage: research-idea
type: collision-synthesis-canonical
date: 2026-06-27
status: 正式彙整（整合三份 round2 火花 + 跨融合，補上 ARC/MDL 與 FSM/自動機兩角度）
source_of_truth: |
  火花真相層＝各論文摘要 ~/repo/pas/paper_reading/summarize/*.摘要.md；
  ai_core 北極星＝~/repo/ai_core/roadmap.md（§2 工廠/消費者、§3 治理、§5 兩層、§6 ATP v0、§7 待決、§8 開放）。
  本檔為碰撞「彙整／呈現層」，非定案規範；規範權威仍在 workflows/spec/ 與 sub_projs/ver_1/try_implement/DECISIONS.md。
integrates:
  - ideas/research/paper_collision_round2_constrained_codegen.md   # (a) 受約束生成
  - ideas/research/paper_collision_round2_program_synthesis.md      # (b) 程式合成/歸納
  - ideas/research/paper_collision_round2_self_improving.md         # (c) 自我改進 agent
  - pas/paper_reading/deep/cross_fusion_aicore_aidesign.md # (d) 三方跨融合（Solomonoff/治理統一）
adds:
  - 角度(e) ARC 當裁判 / 最短程式×Solomonoff 定位：2512.06104 2601.10904 2601.05280
  - 角度(f) FSM/自動機/可微符號×確定性執行：2505.11694 2412.07331 2506.04912 2509.22284 2412.14076
note: 引用一律 [[arxiv_id]] 指回 summarize/。不改 index / session_log。
---

# 論文碰撞・正式彙整版：paper_reading × ai_core

> 這份是把先前四份對撞稿（三份 round2 火花＋一份三方跨融合）**收斂成一份**，並補上兩個尚未被涵蓋的角度——
> **(e) ARC 當裁判／最短程式（Solomonoff/MDL）定位**、**(f) FSM／自動機／可微符號 × 確定性執行層的形式化**。
> 全文圍繞 `roadmap.md` 的一句北極星：**趁貴智能還便宜，讓它生產折舊很慢的符號抽象資產；讓便宜高頻智能去消費；而整圈能不能持續，取決於迴圈裡有沒有一個「不被自己污染的接地訊號」。**

本檔原為單一大檔，因超過拆分門檻而升級成資料夾。內容一字未刪，僅按章節搬移到下列子檔。

## 章節索引

- [i-summary.md](./i-summary.md) — (i) 一句話總結 + ai_core 與論文群最強的 3 條接點
- [ii-sparks-a-c.md](./ii-sparks-a-c.md) — (ii) 依 roadmap 模組組織的火花總表（1/2：A 治理地基、B 受約束生成、C 成本梯度）
- [ii-sparks-d-f.md](./ii-sparks-d-f.md) — (ii) 火花總表（2/2：D 固化引擎旗艦、E 上層抽取/schema、F 角度(e)評測軸引言）
- [ii-fu-iii.md](./ii-fu-iii.md) — (ii-附) 角度(e) ARC/MDL 評測軸專節 ＋ (iii) 建議落地優先序（含 mermaid 依賴圖）
- [appendix.md](./appendix.md) — 附錄：六角度 × 火花對照表
