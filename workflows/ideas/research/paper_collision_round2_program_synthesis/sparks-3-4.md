## 火花 3（旗艦）：wake-sleep 庫學習＝ai_core 固化引擎的現成藍圖

- **來源技術**：
  - **DreamProver [[2604.26311]]**：DreamCoder 式 wake-sleep 搬到形式證明。**wake** 用 LLM 解題、收集「可學習的中間定理」；**sleep** 把中間定理**語意分群（句嵌入＋K-Means＋elbow）→ 抽象出候選引理 → 結構相似度驗證 → 樹編輯距離去重 → 形式驗證 → LRU 遺忘維持 <100 條**。消融鐵證：移除 wake-sleep 精煉 104→76 解、再移除語意分群 76→53（**甚至低於無庫**）——證明「**緊湊抽象庫 > 龐大低階庫**」。
  - **SOAR [[2507.14172]]**：**hindsight relabeling**——任一採樣程式雖對原任務錯，但對「**它自己實際產生的輸出**」必然正確 → 化失敗為訓練對；「搜尋↔學習」閉環抬高整條 scaling 曲線。

- **ai_core 問題**：`roadmap.md §3.6 / §8` 標「**這題優先**」的**固化引擎**——飛輪的免費能量來自「把 LLM 老在處理的同類模糊案例凍成確定性 matcher/snippet」，但**誰來做、自動還是手動，至今未決**。同時「資產庫該多大、怎麼維護不爆炸」也沒答案。

- **具體做法**：把 wake-sleep 直接映成 ai_core 的自動固化引擎——
  ```mermaid
  graph LR
    subgraph WAKE["wake（既有飛輪）"]
      A["笨模型填洞請求"] --> B["ATP trace[] 落 NDJSON<br/>（成功/失敗都記）"]
    end
    subgraph SLEEP["sleep（新增：貴智能定期離線跑）"]
      C["掃 trace NDJSON"] --> D["語意分群<br/>找『反覆出現的同類洞』"]
      D --> E["貴智能抽象成<br/>候選 matcher / snippet asset"]
      E --> F["確定性驗證<br/>（重放歷史 trace 是否仍相符）"]
      F --> G["AST 正規化去重<br/>（呼應火花5）"]
      G --> H["併入資產庫<br/>LRU 淘汰最少用 asset"]
    end
    B --> C
    H -.下一圈更多洞被 matcher 接住.-> A
  ```
  - **資料來源是現成的**：ATP V0 已把 `trace[]` 落 NDJSON（`DECISIONS.md` 2026-06-21、`roadmap.md §6.1`），正好是 wake 階段的軌跡庫。
  - **hindsight relabeling 在 ai_core 的版本**：笨模型一個沒解原任務的 patch，只要過了 `ast.parse`＋簽名閘，它對「自己產生的 (錨點上下文 → 這段碼)」就是一條**合法 few-shot/snippet asset**——把失敗 trace relabel 成資產餵回庫（SOAR 化失敗為訓練資料的 ai_core 翻版）。
  - **「<100 條 + LRU 遺忘」直接回答資產庫維護**：LRU 淘汰＝§3.4 的**撤照**的資料層對應（少用的 asset 退役）。「緊湊抽象庫 > 龐大低階庫」這個被消融強力支持的結論，給「資產庫別無限長」一個硬依據。

- **效益/風險**：效益＝把 §3.6「自動 vs 手動」這道專案最硬的題，從「憑空猜」變成「照 DreamProver 配方落地」；且 wake 資料源（trace NDJSON）已存在，sleep 是純離線批次、用貴智能、低頻——完全契合 §2「資產工廠一次性、貴、稀有」。風險＝(a) DreamProver 引理庫是**領域特定**的（每個框架要各自演化一個庫，不可跨框架共享），(b) 語意分群需要嵌入模型——與 ai_core「純標準庫、零相依」硬約束衝突，**sleep 階段須允許借用貴智能 API 算嵌入**（屬「資產工廠」開機期，可接受），但不能滲進笨模型消費端。

- **roadmap 落點**：`§3.6 + §8`（固化引擎自動版的具體演算法）、資產庫維護策略（<100＋LRU）、`hub.py`（庫的聚集與檢索）。**這是把飛輪從比喻變成可開工管線的關鍵一塊。**

---

## 火花 4：兩層抽象 schema——上層生資產該抽成什麼形狀

- **來源技術**：TheoryCoder-2 [[2602.00929]] 用 LLM **in-context** 合成兩層抽象——**高層 PDDL 運算子**（抽象的前提/效果）＋**低層 Python 謂詞分類器與轉移函數**（把抽象 ground 到具體環境）；唯一人工輸入是**初始 prompt ＋幾個極簡、與實際環境無關的示例**，純粹用來**控制抽象粒度**（讓 LLM 不抽太細也不抽太粗）。跨 episode 重用、成長一個抽象庫。

- **ai_core 問題**：`roadmap.md §7` 的 **Gap B1（語意/用途欄位）**——八/九軸只描述「執行特性」（lifecycle/state/guarantee…），沒有「這個資產**做什麼、吃什麼、什麼時候該用它**」。生資產一定撞到這個缺口；ATP 雖給了 `origin/payload/target` 最小形狀（`DECISIONS.md` 2026-06-21），但「上層該抽出什麼結構」沒有設計依據。

- **具體做法**：照 TheoryCoder-2 兩層結構設計 asset schema——
  - **高層（語意/觸發層）**：類 PDDL 的「**前提/效果**」描述＝「這個資產適用於什麼錨點上下文（前提）、會產生什麼編輯（效果）」。這正是 Gap B1 缺的「用途描述」，也是 §3.4 證書要綁的「適應症」。
  - **低層（grounding 層）**：可執行的 matcher 謂詞（`reads X` / `is_pkg_mgr` 風格，布林函數）＋ snippet 模板。對應 ATP 的 `payload/target`。
  - **借「極簡示例控粒度」**：給貴智能（資產工廠）幾個與目標框架無關的極簡 asset 範本，當「抽象該多粗」的尺——避免它把整個檔當一個 asset（太粗）或每行一個 asset（太細）。

- **效益/風險**：效益＝給 Gap B1 一個有來歷的 schema 骨架（高層語意＋低層 grounding 兩段），且與第九軸證書、固化引擎（火花3 抽出的就是這種兩層 asset）天然吻合。風險＝PDDL 式前提/效果對「程式碼編輯」未必是最自然的抽象語言，可能需要換成更貼近 AST 的描述；「極簡示例控粒度」是工程訣竅、非保證，粒度仍可能漂。

- **roadmap 落點**：`§7 Gap B1`、ATP `asset.py` 的 `origin/payload/target` 擴充、`workflows/spec/` 未來的語意欄位規範。
