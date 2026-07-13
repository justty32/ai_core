## 火花 4 — Wake-Sleep 庫策展 → 上層資產庫的「防膨脹策展器」

| 欄位 | 內容 |
|---|---|
| **來源** | DreamProver [[2604.26311]]（sleep 階段：語意分群→抽象→樹編輯距離去重→驗證→LRU 遺忘，維持 <100 條緊湊庫，證實「緊湊抽象庫勝龐大低階庫」）＋ TheoryCoder-2 [[2602.00929]]（LLM in-context 合成 PDDL 高層運算子＋Python 謂詞低層 grounding，跨 episode 重用成長抽象庫） |
| **對應模組** | `roadmap.md §2/§5` 上層「讀框架→生資產」；元件 4 Function Hub / `thinking_routing.md` Indexer 升級版 |

**具體做法**
1. ai_core 上層會持續生 snippet/few-shot/骨架，但**沒有資產庫管理故事**——膨脹後反而塞爆下層笨模型的 context（與火花 1 的「精簡參考」衝突）。引入一個 `idea curate` 子命令（聰明模型，one-shot，低頻）：
   - 對累積的 asset 依 `ast` 結構相似度分群（DreamProver 的語意分群）；
   - 每群抽象成一個**參數化模板**（TheoryCoder-2 的「高層 PDDL 運算子＋低層 Python 謂詞」二層＝ai_core 的「snippet 骨架＋確定性驗證謂詞」二層）；
   - 用樹編輯距離去重，LRU 遺忘長期沒被 `trace[]` 命中的 asset，維持庫緊湊。
2. 緊湊庫直接餵火花 1 的「精簡參考」與火花 5 的「文法約束」。

**預期效益／風險**
- 效益：[[2604.26311]] 實證緊湊抽象庫解題數遠勝堆疊低階庫（104 vs 53）；對 ai_core 意味著「下層每次填碼吃的 context 更小、更便宜」，直接省 token＝省錢（北極星）。
- 風險：抽象過度→模板太通用反而要笨模型填更多自由度（違背 `roadmap.md §5`「自由度愈小愈不會搞砸」）；用 `trace[]` 命中率回饋控制抽象粒度，沒人用的抽象就 LRU 掉。

**roadmap 落點**：元件 4 Function Hub＋Indexer 升級版（`thinking_routing.md`）；連動 `DECISIONS.md B1`（語意欄位是策展的輸入）。

---

## 火花 5 — 文法約束槽填充＋機率枚舉 → 下層「窄面填碼」的本質升級

| 欄位 | 內容 |
|---|---|
| **來源** | GridCoder [[2411.17708]]（條件獨立假設攤平 DSL token 搜尋空間，只需幾次神經推論＋機率枚舉）＋ NLI [[2604.18907]]（Inductor 學離散 token 程式＋Interpreter 遞迴可微執行，長度外推 OOD 99%） |
| **對應模組** | `roadmap.md §5` 下層受約束生成；v0 `line_assistant.py`（錨點定位）＋`lib/skeleton.py`（`ast` 裁剪鄰域） |

**具體做法**
1. v0 下層目前是「把 snippet 寫進 prompt＋行數助手標位置，讓笨模型一次寫出」——仍是自由文字生成。升級成**文法約束槽填充**：
   - `skeletonize()` 已保留錨點鄰域的 AST → 從中導出一個**小型文法／選擇集**（這個槽合法的型別、可用的已抽取 API 名、snippet 變體），當作 GridCoder 的「DSL token 集」。
   - 笨模型不再自由生碼，而是對每個槽輸出一個**在合法選擇集上的機率分佈**（受約束解碼的弱化版：用 prompt 限定「只能從這張清單選」）。
   - ai_core 做 GridCoder 式的**機率排序枚舉**：依機率展開 top-k 候選，每個用 `ast.parse`＋簽名＋`execute_in_isolation` 確定性驗證，第一個過的即採用。
2. NLI 的啟示：把填碼看成「離散骨架（聰明模型給）＋槽內細節（笨模型給）」的二層，遞迴套用到巢狀結構（函式內的子表達式）。

**預期效益／風險**
- 效益：把「自由度爆炸→必錯」（`roadmap.md §5` 對笨模型的核心擔憂）真正收斂成「在有限合法集上選」，自由度由文法決定而非提示語氣；GridCoder 證明這種枚舉只需極少推論次數＝便宜。
- 風險：合法選擇集若不全 → 正解被排除在外（漏接）；保留一條「raw 自由生成」退路（對應 `pruning.strategy=raw` 對照組），文法只當主路。

**roadmap 落點**：`roadmap.md §6` v0 的 `line_assistant.py`／`skeleton.py` 升級；這是下層受約束生成最直接的演算法骨幹。

---

## 火花 6 — Induction 優先 / Transduction 兜底 → per-request 成本梯度的具體機制

| 欄位 | 內容 |
|---|---|
| **來源** | BARC [[2411.02272]]（分別訓歸納模型〔合成程式＋範例過濾〕與傳導模型〔直接神經預測〕；集成＝歸納優先、無解時傳導兜底，互補強）＋ SYNTRA [[2509.17393]]（LLM 當偽標籤器＋greedy maximin 主動選最具判別力測試，省一半呼叫） |
| **對應模組** | `roadmap.md §2/§3.3`「先笨模型填、沒過驗證再升級聰明模型」的 per-request 成本梯度；ATP `degradation.tier` |

**具體做法**
1. `roadmap.md §3.3` 把成本梯度講成抽象的「動態縮放洞」，BARC 給出可實作的二層集成：
   - **路徑 A（induction，便宜）**：笨模型走火花 5 的文法約束填碼，產出片段→過確定性驗證（範例過濾）。
   - **路徑 B（transduction，貴）**：A 連續失敗 N 次 → 升級聰明模型直接重寫整段（傳導式直接預測）。
   - `degradation.tier` 記錄哪一層成功，寫進 ATP，餵給火花 3 的固化決策（某類案例若 A 永遠失敗，要嘛新 matcher、要嘛永久劃為聰明模型領地）。
2. SYNTRA 的省呼叫技巧：當有多個候選片段難分時，不要逐一全測，用 greedy maximin 選「最能淘汰最多候選」的那個確定性測試先跑，省驗證成本。

**預期效益／風險**
- 效益：把成本梯度做成可量測的 tier 切換，且每次切換都產生「該案例笨模型行不行」的數據——這正是飛輪（`roadmap.md §3.5`）需要的燃料。
- 風險：升級門檻 N 設太低→過早燒聰明模型（貴）；設太高→使用者等太久。用 `trace[]` 統計各案例型態的歷史成功 tier 自適應調 N。

**roadmap 落點**：`roadmap.md §3.3` 動態縮放洞落地；ATP `degradation` 欄；`DECISIONS.md A4`（組合軸／tier 推導）。
