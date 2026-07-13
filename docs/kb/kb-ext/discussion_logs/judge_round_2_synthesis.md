# 事實判斷函數圓桌 Round 2：主持人收斂

> 把四份發言收斂成一份「訓練零錯誤 true/false 函數」的設計與方法論。這場是全系列最深的一場——它把 judge 這個最小函數，變成 roadmap §3.6 固化引擎、第九軸證書、飛輪 的**第一個可操作實例**。

## 0. 一句話結論

**「訓練一個零錯誤的 true/false 函數」＝把含 LLM 留白的函數，用一組 I/O pairs 當 test_set，固化成「規則層 + override 表 + LLM fallback」三層結構，直到 test_set 全綠。** 而「零錯誤」精確地說是 `nondeterministic: {test_set, stability:100%}`——**第九軸證書本身**。四人獨立收斂到**完全相同的三層三明治**，這個高度一致本身就是設計成立的強訊號。

## 1. 四人零分歧的共識：三層三明治

```
input x
  │
  ├─ 層1  確定性規則 rules        覆蓋多數、可泛化、可稽核、便宜
  │       (聰明模型歸納的 f(x)->bool)   命中→true/false ；miss→sentinel 往下落
  │                                     ＝已固化的確定性程式碼（footprint 在這長大）
  ├─ 層2  override 例外表          規則覆蓋不到的殘差，死記
  │       (memoize: x→label)            記過→死記值；沒記過→sentinel 往下落
  │                                     ＝機械保證「零錯誤 on train」的安全網
  └─ 層3  LLM fallback             沒見過的新輸入，模糊前沿
          (bind+retry+vote+guard)       唯一非確定、唯一未認證、唯一帶證書的點
  │
  ▼  is_true_or_false 邊界門 → 嚴格 true/false（強制約束1，不靠 prompt 祈禱）
```

用 `compose.with_fallback` / `route` 串起，全部用現成積木（`memoize` + `compose` + `bind`），**不造任何新輪子**。這三層直接映射 roadmap：層1＝已固化確定性碼、層2＝固化半成品（死記未歸納）、層3＝§3.4 模糊前沿暫時處理者。

## 2. 五個核心洞察（跨專家共識）

| # | 洞察 | 提出/論證 |
|---|------|-----------|
| **C1** | **「零錯誤」＝ stability=100% on THIS test_set ＝第九軸證書本身**。含 LLM 的函數不可能絕對零錯誤；證書的價值不在「100%」這數字，在它**逼你寫下 test_set 是什麼**。把 100% 讀成絕對保證＝吃掉 test_set 限定詞的危險誤讀 | SGA（「使用者用六個字說出了第九軸存在的全部理由」），全員 |
| **C2** | **train 零錯誤是 trivial 且危險的**——memoize 查表是它的解析解，「train 全綠」信息量是零，分不出「學會了」和「背下來了」 | AIRE，SSE 印證 |
| **C3** | **override 表的大小＝過擬合的溫度計**。`|override|/|train|` 是死記佔比，一個訓練時就能數出的整數。**把「泛化」這抽象的好東西，換成可被最小化的 metric＝把奧坎剃刀做成工程** | AIRE（整套方法的支點）|
| **C4** | **硬零錯誤 vs 泛化不必打架**：規則層只管泛化（容許它在 train 漏判）、override 層兜住所有漏判→整體**無條件達成 train 硬零錯誤**，代價只體現在 `|override|` 大小。約束(2)從不必犧牲泛化 | AIRE，四人結構一致 |
| **C5** | **holdout 是區分 trivial 解 vs 有價值解的唯一裁判**。train 上兩者都 100%；只有沒見過的 held-out 能照出「純查表 holdout≈瞎猜 vs 規則泛化 holdout 仍高」 | SSE+AIRE |

## 3. unknown「移位」——三人同構的最漂亮收斂

使用者廢掉上輪的 unknown 第三態。三位專家從不同路徑得到**同一個結論：unknown 沒消失，它移位了**——

- **SSE（控制流視角）**：對外廢掉 unknown（它是「逃避責任的後門」，會餓死飛輪——判錯訊號是訓練迴圈的燃料）；對內保留 `UNDECIDED` sentinel（層1/2 棄權往下落），在層3 被 LLM 強制收斂成二元而消失。**廢掉作為輸出的 unknown，保留作為內部控制訊號的 sentinel。**
- **SSA（固化進度視角）**：unknown 是**「留白尚未在此點消除」的內部信號**，精確對應第九軸還沒從 true 升證書的輸入——**unknown 就是「飛輪還沒轉到」的座標**。顯式化它，訓練進度（coverage↑、unknown 比例↓）才有梯度可循。
- **SGA（治理邊界視角）**：unknown 從**輸出層移位到定義域邊界**。集內每個 pair 有確定標籤（純二元成立，使用者贏），「資訊不足」變成**集外＝未認證區**，由「證書 test_set 顯式邊界 + 集外預設不信任」承接。

**統一裁決**：對外**嚴格二元**（使用者約束1），對內以 sentinel / 證書邊界承認無知。純二元（**輸出契約**，集內成立）與不信任（**邊界契約**，集外接管）是兩層不同契約，不矛盾——上輪的錯正是想用一個 enum 值同時扛這兩層。

## 4. 「訓練」作為元函數 + 第九軸躍遷 + 兩種證書

**「訓練」是一個收在代數內的元函數**（SSA）：`train: (judge_skeleton, [io_pairs], policy) → forged_judge_asset`。它是 roadmap「資產工廠」的字面落地，比組合子高一階（組合子 runtime 黏函數，train 固化期把留白塌縮成確定函數並寫進 SFC store）；但對外仍是一個宣告 `--metadata` 的 CLI，hub 掃得到、可被組合。**「資產工廠本身也是積木」是飛輪能轉的結構前提。**

**第九軸躍遷**：
```
訓練前(骨架)：nondeterministic: true
訓練後(資產)：nondeterministic: {model, test_set, stability:1.0, coverage:?, holdout_stability:?}
guarantee:    idempotent  ⟺  第三層 LLM fallback 已死（留白歸零）；否則維持 none
```

**證書光記 stability 會撒謊（SSA+SGA 共同的硬線）**：trivial 查表與泛化規則在 test_set 上都 stability=1.0，但可信度天差地別。故證書**必須加 `coverage`（規則層命中比例）**——coverage 高＝真泛化、可降階成確定原子、可當別人的 validate；coverage 低＝本質還是查表、**不該在飛輪裡降階**。**輸出可以騙人地一致，證書不准騙人地一致。**

**兩種證書 → 兩種撤照（SGA）**：
| 產物 | 證書語意 | 撤照觸發 | 處理 |
|---|---|---|---|
| 查表死記 | exact-match on N inputs，邊界銳利 | 新 pair **查表 miss**（查無） | 補進表 N→N+1，**擴張、無痛、不重訓** |
| 歸納規則 | stability% over domain，邊界模糊 | 新 pair **答錯**（反例證偽） | 證書失效、回 §3.6 **重訓**或降級為混合（roadmap `rejected{retry_exhausted}`）|

## 5. 統一的 train 設計與評測（可開工）

**訓練迴圈（失敗驅動 TDD，SSE）**：餵 pairs → EVAL（跑全部、記殘差 R）→ R 空則簽證書；R 非空則 REPAIR（(a)殘差有共同結構→加規則 (b)零散→丟 override (c)規則寫不出→調 prompt）→ **重跑全部 pairs（防迴歸）**。

**三條 override 紀律（SSE）**：① override 在規則**之後**、LLM 之前（擺最前面會讓規則永遠長不大、退化成純查表）；② override 每筆是「待固化的工單」，**永不縮小的 override 表是設計失敗的氣味**；③ 進過 override 的 key，對應規則必須回 sentinel 棄權，**不可硬猜**（規則自信答錯時 override 救不了）。

**規則歸納 + 奧坎篩選（AIRE，走現成 rail）**：複用 idea.py 的 `bind→entry manager→backend` 加 `induce` stage，system prompt 硬性要求「最短確定性規則、輸出可執行 `def rule(x)->bool`、禁止 literal 死記」；出候選規則集，**用 dev 錯誤率選、description length 破平手**（剃刀落地）；train 全綠但 dev 崩的規則＝死記，直接淘汰。

**評測（AIRE）**：分層切 train(70)/dev(15)/test(15)，test 封存只量一次。報三個數字：`test_error`（真實泛化）、`|override|/|train|`（過擬合溫度計）、`rule_description_length`（資產耐久性，越短越能被未來笨模型消費）。**選規則只准看 dev，碰 test 即作弊。**

## 6. 這場回答了 roadmap §3.6 的什麼

§3.6「固化引擎手動 vs 自動」是 roadmap 標「這題優先」的最硬未決題。這場給出了**最小可操作的半自動固化**：
- **固化的單位**＝一個 case 從層3（LLM）→層2（override）→層1（規則）的遷移。
- **固化的驅動力**＝失敗驅動迴圈（殘差 R 就是待固化清單）。
- **固化的度量**＝`|override|` 縮小、`coverage` 上升、`unknown 比例` 下降＝飛輪從「寬鬆 regime」往「嚴格 regime」轉的可測梯度。
- **手動 vs 自動的折衷**＝**半自動**：聰明模型提案規則（自動），失敗迴圈機械驗收（自動），但「哪些殘差該歸納成規則」目前靠人/聰明模型看（半手動）。這正是 §3.6 該有的 v0 答案——不押注全自動，但給出可量測的固化梯度。

> 副產物：這場逼出第九軸證書應加 **`coverage` 欄位**（區分泛化規則 vs 藏起的查表），建議回填進 `axis_spec.md §9` / `lib_spec.md §9` 的證書 schema——這是對既有規範的具體增補建議。

## 7. 下一步

- **judge + train 是固化引擎的最小 demo**：約 40 行 judge + 一個 train 迴圈（memoize+compose+bind），可作為 ATP v0 開工後**第一個能展示飛輪轉動**的可跑實例（layer3→layer2→layer1 遷移肉眼可見）。
- **規範增補建議**：第九軸證書 schema 加 `coverage` / `holdout_stability`（待你定奪是否扶正進 `core_nature/`）。
- 或繼續圓桌下一主題。

---
*產出方式：四份發言由內部並行 subagent 獨立扮演四位專家產生；收斂由主持人（Claude）撰寫。本輪未動 `_core.py` / `core_nature/`，但提出第九軸 `coverage` 欄位的增補建議。*
