---
title: 論文碰撞・第二輪——程式合成／歸納 × ai_core 抽取+受約束生成
stage: research-idea
type: brainstorm-collision
date: 2026-06-27
sources:
  - 2411.02272  # BARC 歸納vs轉導
  - 2411.08706  # LPN 潛在程式搜尋
  - 2411.17708  # GridCoder
  - 2507.14172  # SOAR 自我改進閉環
  - 2509.17393  # SYNTRA 轉導式測試時合成
  - 2507.16405  # Poker 自監督 ILP
  - 2606.24245  # AutoSpec ILP 護欄演化
  - 2604.26311  # DreamProver wake-sleep 引理庫
  - 2602.00929  # TheoryCoder-2 抽象學習
  - 2501.17848  # eggp 等式圖 GP
  - 2604.18907  # NLI 神經詮釋語言
provenance: 由外部 agent 讀 paper_reading/summarize + deep/self_improving_program_synthesis.md 與 ai_core/{roadmap,CLAUDE,thinking_*}.md、sub_projs/ver_1/try_implement/DECISIONS.md 後產出。真相層仍是各論文摘要與 roadmap.md；本檔為碰撞火花，非定案。
---

# 論文碰撞・第二輪：程式合成 × ai_core

> **碰撞主軸**：ai_core 的第一目標問題（程式碼輔助助手）正好＝程式合成社群的兩個經典子問題——
> **上層「讀框架→生資產」＝結構化抽取／庫學習**，**下層「精準標記位置填碼」＝受約束生成（programming-by-example）**。
> 程式合成圈這十年的硬核成果，幾乎都能直接對接 ai_core 的具體待決規範（`roadmap.md §6/§7`、`DECISIONS.md` A4/B 系列、ATP v0）。
>
> 一個貫穿全篇的關鍵對位：ai_core `roadmap.md §3.2` 把系統描述成「**確定性前濾網（matcher）→ 命中走純程式碼；落穿才掉進 LLM（cache-miss / `switch default`）**」。
> 用程式合成的語言講，**matcher＝歸納（induction，可執行可驗證的程式），LLM 留白＝轉導（transduction，直接生成）**。
> 也就是說——**ai_core 的核心架構，本來就是 BARC [[2411.02272]] 那組「歸納⊕轉導」二分的工程化身**。這讓下面 7 個火花有共同地基。

## 章節索引

- [sparks-1-2.md](sparks-1-2.md)——火花速覽表（7 個火花總覽表格）＋火花 1（歸納⊕轉導雙軌填洞）＋火花 2（多候選 + 貴智能偽標籤篩假設）。
- [sparks-3-4.md](sparks-3-4.md)——火花 3（旗艦，wake-sleep 庫學習＝固化引擎藍圖）＋火花 4（TheoryCoder-2 兩層抽象 schema）。
- [sparks-5-7.md](sparks-5-7.md)——火花 5（等式圖語意去重）＋火花 6（執行引導填洞＋條件獨立攤平）＋火花 7（旗艦，ILP 學判別謂詞）。
- [closing.md](closing.md)——誠實排除（LPN / NLI 為何此刻不採用）＋收束（兩條可立即開工的線）。
