# 固化引擎圓桌 Round 1：通用形態

> 主題：ai_core 的**固化引擎（crystallization engine）通用形態**——roadmap §3.6 標「這題優先」的最硬未決題。
> 框定：固化＝飛輪的**撤照支**（把「LLM 老在處理的同一類模糊案例」凍成確定性 matcher/分支）。核心抉擇：**誰做？手動（好用的工具）vs 自動（會自我改進的系統，難非常多）。目前未決。**
> 已有抓手：judge 訓練那輪＝固化的最小實例（三層三明治、override 溫度計、coverage 欄位、半自動）。
> 四位發言由內部並行 subagent 獨立產出——**四人一致認為「換規則」已被解掉，真正的難題在「時機判斷」與「治理」。**

---

## SSE（通用固化引擎的最小可跑形態）

固化引擎本質是一台**離線批次處理機**：吃 trace log（input→output 配對），吐確定性分支。但 judge 那輪能跑是因為原料是**乾淨的標註資料集**（人手交的已知正解 pairs）；通用固化沒這待遇——原料是**野生 production trace**，那個 output 是不是對的**沒人保證**。這是 judge 特例與通用情形之間最關鍵、最常被跳過的裂縫。

**原料缺口（硬發現）**：讀了 `lib/trace.py`——它記的是**調用樹的拓撲與時序**（trace/span/parent/name/ts/duration），是 observability 原料，**不記 payload**。`_emit` 的 `**fields` 能塞任意欄位但目前沒有約定記 LLM 的 input/output。**結論：trace.py 現在不夠當固化原料，缺的正是 I/O pairs 本體。** 要補的不是重寫，是加一條約定：LLM 環節的 span 記 `io={"in":…,"out":…}`（大 payload 比照 memoize 存 digest+旁路檔）。補完後「trace 裡的 input→LLM output 配對」**就是** judge 的 I/O pairs，差別只在 judge 那組有人背書、trace 這組對錯未知——這差別決定通用固化必須多一道**正解來源**設計。

**五步流程（每步掛現成積木）**：① 採集（bind 在呼叫前後用 trace.span 記 io）→ ② 聚類偵測「高頻+output 穩定」的候選桶（複用 vote 的 key 正規化；**這是把 judge 的 |override| 溫度計反過來用**）→ ③ 聰明模型提案確定性規則（經 entry manager，產出＝SFC tiny function 定義）→ ④ 歷史 trace 當測試組機械驗收（coverage + agreement，**測試組是白來的**）→ ⑤ 人/聰明模型看殘差（半自動的「半」，v0 不能消去）。

**替換機制其實已解完**：固化產出規則後怎麼插回去＝ `compose.with_fallback(primary=新規則, fallback=原 LLM 環節)`。固化是遞增的：`with_fallback(rule2, with_fallback(rule1, llm))`，層1越長、LLM 越退後。**不需要任何新組合子**，with_fallback/route/guard 三件齊了。唯一要補：把 fallback 鏈**持久化進 SFC store**（否則重啟蒸發），sfc.py persist 承接。

**v0 立場：先全手動，只證明洞填得準**。理由＝方法論剎車：v0 切片的命題裡**根本還沒有需要固化的對象**（固化是飛輪轉幾圈後才有意義，v0 階段 trace 還是空的，自動掃 log 是在優化還不存在的瓶頸）。更實際的風險：自動固化藏著「會撒謊的證書」（coverage 低的查表式規則 stability=1.0 卻是過擬合），沒有 holdout 當唯一裁判會把死記偽裝成泛化、自動降階進飛輪——**反向污染系統可靠性，比沒做固化糟得多**。v0 只要兩樣：(a) **trace 加 io 欄位**讓原料開始累積（廉價、且不做以後補不回來——歷史 trace 丟了就丟了）；(b) 手動固化走一遍。**先證明洞填得準（手動），再證明洞能自動被發現（半自動）——順序不能反。**

```
[生產期·線上]                          [固化期·離線批次]
 bind→LLM 環節 ──trace.span(io)──► Collector 撈 I/O pairs
   ↑ fallback                          ↓
 with_fallback ◄──持久化進 SFC──   ② 偵測高頻+穩定候選桶(|override|溫度計反向)
 (primary=規則,                        ↓ 餵候選 pairs
  fallback=LLM)                   ③ 聰明模型歸納規則(→SFC fn 定義)
                                       ↓
                                  ④ 歷史 trace 當 test_set 驗收(⚠ holdout 是唯一裁判)
                                       ↓ 不過→殘差
                                  ⑤ 人/聰明模型看殘差(半自動的「半」)
── v0 範圍：只做①採集(trace 加 io,不可後補) + 手動走③④ ──
```

---

## SSA（固化引擎作為系統元件的架構）

固化引擎是飛輪的**撤照側阻尼器**。但先分清層級：judge 的 `train: (skeleton,pairs,policy)→forged_judge` 是**一個資產工廠函數**（收在代數內、帶 --metadata、可被 hub 掃描）。「固化引擎」這詞被用來指兩種規模差異巨大的東西——(一)「把這一個帶留白的函數凹成全綠」（＝train 推廣）、(二)「掃描整個生態、跨函數找哪些 LLM 環節值得固化、提案、排程」。**前者是元函數，後者是元函數的編排器。把它們混為一談正是這題「最硬」的原因——讓人以為要設計一個無所不能的大引擎，違反 KISS。拆開後大部分難度蒸發。**

**固化引擎不該是 monolith，而是「train 家族 + 掃描器 + 排程器」的組合，三者都是普通 ai_core 函數**：`scan(生態)→候選清單` ∘ `prioritize(候選,trace頻率)→排程` ∘ `train(目標,pairs,policy)→資產`。元件分工固定：**trace＝原料唯一來源**（每次 LLM 留白命中是一條 span，accumulate＝pairs 礦）；**SFC store＝產物唯一倉庫**（forged judge 是一個 tiny function，compile_function 直接載入）；**第九軸 metadata＝證書唯一帳本**；**hub 升級版＝唯一生態掃描器**（讀第九軸把 `nondeterministic:true` 標成固化候選、交叉 trace 頻率排序）；**indexer 不變**（純 --metadata 彙整）。

**方向性硬規矩：固化是單向「LLM 領地→確定性領地」遷移，但證書狀態是可逆的雙向狀態機。** 不矛盾——「程式碼資產」和「准入狀態」是兩個東西：固化產生的規則/override 是**永久累積資產**（永不刪，刪了等於丟棄聰明模型一次性付的成本）；但 forged judge 領的證書 stability **會掉**（holdout 擴大、分布漂移、coverage 被發現虛高）。**撤照≠刪資產，是狀態回退**：`guarantee:idempotent→none`、`nondeterministic:證書→true`、重新掛上 LLM fallback 層。三明治天生支援——層3 固化後不是被刪，是**被旁路**；撤照只是把旁路打開。故第九軸 dict 應記 `forged_from`/`asset_ref` 指回骨架，讓「撤照的撤照」無損可逆。

**四題結論**：① 固化引擎＝train 元函數的**高一階編排器**（scan∘prioritize∘train 的 chain，非 monolith）。② trace 原料 / SFC 產物 / hub 升級版掃描器 / indexer 不變 / 第九軸證書帳本。③ **資產單向（只進不退）、證書雙向**（true ⇄ {證書}，撤照＝旁路重開 LLM 層不刪資產，第九軸加 forged_from）。④ 固化引擎**是** ai_core 函數、遞迴閉合（hub 掃 hub、train 能固化 train 內部的 judge）——**但 v0 踩剎車：固化引擎的「准入閘門判斷」應保持 nondeterministic:true 不自我固化**，因為治理要求閘門要慢、可審計。**讓飛輪轉，但別讓飛輪自己決定要不要拆掉自己的剎車。**

```
indexer(不變)──彙整──► hub(升級版掃描器:挑 nondeterministic:true + trace 頻率排序)
                              │ 待固化候選清單(資產)
trace(原料:io pairs礦)──────► train(元函數工廠:三明治凹到全綠)──► SFC store(產物倉庫)
                                                                      │ 寫證書
                              第九軸帳本: true ──固化──► {model,test_set,stability,coverage,holdout,forged_from}
                                                  ◄──撤照(雙向,旁路重開LLM,不刪資產)──
  資產只進不退(永久累積) · 證書雙向狀態機 · 遞迴閉合但 v0 不自固化准入閘門
```

---

## SGA（固化的治理——憑證准入的引擎，也是唯一不可撤銷的危險動作）

「飛輪的免費能量」只對了一半。**固化沒有免費——它只是把成本從現在挪到未來、從可見挪到不可見。** 固化是 §3.4 憑證准入的**發照手**：撤掉 LLM 那張照、改發一張「確定性程式碼」的永久照。其他所有閘門都在攔截 LLM；**只有固化在主動擴張確定性領地**。所以它必須受比任何 LLM 環節更嚴的看守。

**最關鍵的不對稱（整段地基）：錯誤固化的傷害嚴格高於 LLM 犯錯**，三層——(a) **可見性**：LLM 輸出帶懷疑光環（guard/retry/人類本能兜底）；固化後的確定性分支自帶「確定性程式碼」可信外觀，沒人對 `if x in TABLE: return True` 起疑，錯誤是 **silent**。(b) **監督退場**：飛輪終態是 LLM 退縮，**一旦某環節固化、LLM 退場，盯著它的智能也退場了**——錯誤 LLM 輸出有下一輪重跑，錯誤固化規則**沒有下一輪**。(c) **永久**：成熟期連「重新審視這條規則」的智能都可能不再便宜。**固化是這套系統裡唯一同時 silent+永久+監督退場後發生的傷害。** 故固化的預設姿態必須是**拒絕**（與 LLM 進場同預設、反向）。

**固化閘門（最小集，缺一不可）**：注意反轉——§3.4 兩道閘門是「准 LLM 進來」，固化是「准把 LLM 換掉」。(1) **泛化證據不是查表證據**——`stability` 在 test_set 100% 是**零信息量**（查表死記和真規則都全綠）；准許固化的條件必須掛 **coverage 和 holdout，不掛 stability**。低 coverage＝藏起的查表，不准降階成確定性原子。(2) holdout 強制通過（唯一裁判）。(3) 證書四欄 `{model,test_set,stability,coverage}` 完整，缺 coverage 視為**偽造照**拒收。(4) 簽核。**閘門掛在 coverage/holdout，不掛在 stability；光憑 train 全綠就固化＝憑偽造證書發永久照。**

**可逆性（不可妥協）**：反對「固化＝刪掉 LLM fallback、原地覆蓋」。回滾三件套：(a) **保留被替換的 LLM fallback 作休眠影子分支**，固化只改路由不物理刪除，撤照＝把路由切回層3；(b) 固化分支帶**反例計數器**作撤照觸發；(c) **兩種撤照分流**——查表 miss（集外新輸入）＝**無痛擴張**（補 pair、可線上）；規則答錯＝**證偽 incident**（意味當初 coverage 證書撒謊、騙過 holdout，必須跳閘、降級回 fallback、證書標記待重認證）。`guarantee:idempotent ⟺ LLM 層已死`——故 **v0 建議 fallback 層永遠不要真死，最多休眠，以保回滾能力**。

**手動 vs 自動的紅線**：把「半自動」說得更狠——**證據的生產可以全自動，領地的擴張不可以全自動。** 自動允許：掃 log、歸納候選、跑 holdout、算 coverage、填證書草稿（產物是**待審提案**，不動領地邊界）。v0 永遠需要蓋章、不准全自動：(1) 會物理刪除 fallback 的固化；(2) coverage 低於門檻仍想固化（查表偽裝成規則，最危險）；(3) 對同一座標的二次以上重新固化（有撤照前科）。唯一可全自動：**override 表的無痛擴張**。對應 coding agent 那輪——**正常終態＝待審提案，落盤需授權。**

---

## AIRE（固化的時機判斷——成熟度才是難題，換規則只是工程）

固化的難不在「怎麼把 LLM 換成 `def rule`」（Round 2 已解），在**時機**：一個案例到底成熟了沒？**太早固化，你不是省下 LLM 成本，你是把飛輪的免費能量換成一條 silent 的脆規則**——它在 train 上 100%、在你看不見的 holdout 上悄悄崩，沒有任何 runtime 報錯。這就是自動固化「難非常多」的原因：**自動換規則容易，自動判斷成熟度＝自動做統計推論，而歸納樣本又少又偏。**

四個信號都已是可操作的 runtime 量：trace（樣本來源）、memoize 的 hit 結構（同輸入多次一致＝穩定度的廉價量法）、Round 2 的 |override|/coverage/holdout。

**就緒度判據＝四道 AND 閘（不是 OR）**：`ready = (n≥N_min) ∧ (stability≥θ) ∧ (coverage≥c_min) ∧ holdout_ok`。
- **樣本量閘** n≥N_min（v0 取 30，低於此連 holdout 都切不出、是三點擬合一條線）
- **穩定度閘** 同 input hash 的 LLM 一致率≥θ（0.95）——**不穩的輸入本質沒有確定答案，固化它＝脆化，一票否決**
- **coverage 閘**（規則命中比例≥c_min，低＝藏起的查表，**測謊器**）
- **holdout 閘**（封存 trace 上 stability 不顯著低於 train，**唯一能區分學會 vs 背下來**，選規則只准看 dev）

**把 ready() 寫成函式，它本身就是固化引擎的核心判官，可被 hub 掃描、可被另一個 train 當 validate（判斷器判斷判斷器）。**

**過早固化＝把噪音當規律**，與 ML 過擬合嚴格同構（trace 是 survivorship-biased 樣本，可能把「LLM 偶然答對」固化成 train 上 100% 的錯誤規則）。數據防線：holdout（最後也最硬，碰 test 即作弊）+ N_min（擋三點擬合）+ 簡潔度門檻（**rule_description_length 升格成硬閘——超長直接不准固化，長規則＝在 encode 殘差，那是 override 的活**）+ |override|/|train| 溫度計。

**固化 vs 留 LLM**：該固化＝`高頻 ∧ 穩定 ∧ 規則簡潔`；該永遠留 LLM＝`低頻 ∨ 不穩 ∨ 規則必然複雜`。**固化一個本質模糊的東西＝用脆規則假裝確定，比誠實留給 LLM 更糟。留白不是失敗，是對「這裡確實模糊」的誠實宣告——飛輪只該吃確定能凍住的能量，硬吃模糊的會把自己噎住。**

**量固化成效（A/B 對照）**：`save=freq×per_call_cost`（收益，RateMeter 量得到）、**holdout 準確率有沒有降（淨贏 vs 脆化的分水嶺，一票否決——規則 holdout 顯著低於 LLM holdout 就回滾，省再多錢都不算贏）**、coverage、規則耐久性（description_length，時間維度的脆化風險）。確認淨贏＝`固化後 holdout ≥ 固化前 LLM holdout − ε 且 save>0 且 coverage 合理`，三者皆滿足才簽證書、才讓 LLM 層死掉。

**對 §3.6 的精確答案**：半自動的「半」**不在規則生成，在就緒度門檻**——機器能自動量四個信號、自動否決過早固化，但門檻值（N_min/θ/c_min/ε）由人或聰明模型在 v0 校準。**先把 ready() 守門器做出來，固化引擎才敢開始自動轉。**
