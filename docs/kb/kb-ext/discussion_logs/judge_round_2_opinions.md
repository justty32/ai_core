# 事實判斷函數圓桌 Round 2：訓練一個零錯誤的 true/false 函數

> 使用者補充三約束，主題升級：(1) 嚴格只吐 true/false（推翻上輪 unknown）；(2) **不能有任何判斷錯誤**（零錯誤）；(3) 達成方式＝**「訓練」**：給一群 I/O pairs 把函數「凹」到完全符合。
> 核心 framing（主持人開場）：在 ai_core 哲學下，「訓練」＝用聰明模型/確定性規則把含 LLM 留白的函數固化到 I/O pairs 全綠——這是 roadmap §3.6 固化引擎 + 第九軸證書 + 飛輪 的最小可操作實例。「零錯誤」只對給定 pairs(test_set) 成立。「凹到全綠」有 trivial 解（查表死記=過擬合）與有價值解（歸納泛化規則）。
> 四位發言由內部並行 subagent 獨立產出——**四人不約而同收斂到同一個三層結構**。

---

## SSE（訓練迴圈怎麼真的跑起來）

先把「訓練」從 PyTorch 拆掉：這裡的訓練是**失敗驅動的固化迴圈**＝TDD——餵 I/O pairs → 跑當前函數 → 蒐集判錯的 pair → 針對殘差修 → 重跑直到全綠。pairs 就是測試組，「全綠」就是 stability=100% on test_set。**釘一句在桌上：「零錯誤」永遠是 on a given test_set 的零錯誤，不是形上學的零錯誤**；trivial 解（查表）和有價值解（泛化）在「on train 全綠」這指標上**完全等價，指標分不出它倆**——要分，得靠第二個沒見過的 holdout。

**訓練迴圈（每步配現成積木）**：
```
I/O pairs(train) → EVAL: for (x,expect): got=judge(x); 記錄 got≠expect 的殘差集 R
   R 空? ─yes─► 全綠，簽證書 {model,test_set,stability=100%}
   R 非空 ─► REPAIR 三選一：(a)殘差有共同結構→加確定性規則 (b)殘差零散/矛盾→丟 override 例外表死記 (c)規則寫不出→調 prompt/few-shot
   ─► 回 EVAL（重跑【全部】pairs 而非只重跑 R——防迴歸，TDD 紀律）
```
積木對應：EVAL＝for loop；**override 表＝ `memoize.py` 但語意改寫**（原是「為效能不為正確性」，這裡徵用當**正確性層**：key=canonical(x)、value=死記標籤、不失效、version 綁 test_set_hash）；「規則 miss 就落 LLM」＝ `compose.with_fallback`；LLM 留白＝ `bind` 一個「只准回 true/false」的 system prompt；馴化 LLM 的吵＝ `retry_until_valid`/`guard`/`vote`。

**三層三明治（最務實的「凹到全綠」結構）**：
```
input → 層1 確定性規則(覆蓋多數、可泛化、可稽核) ─miss(回 sentinel UNDECIDED)→
        層2 override 例外表(memoize，殘差死記) ─miss→
        層3 LLM fallback(bind+retry+vote+guard，唯一非確定/未認證點) → true/false
```
直接映射 roadmap：層1＝已固化的確定性程式碼（footprint 在這長大）、層2＝固化半成品（死記但未歸納成規則）、層3＝§3.4 模糊前沿暫時處理者（唯一帶證書的點）。**飛輪在這圖上肉眼可見：訓練每跑一輪，就有 case 從層3→層2、層2→層1 遷移。**

**零錯誤怎麼機械保證——override 兜底我接受，但下三條紀律**：① **位置**：override 必須在規則【之後】、LLM 之前；擺最前面當總攔截，規則就永遠跑不到、退化成純查找表（trivial 解的退化形態）。② **角色**：override 每一筆是「待固化的工單」，不是終點；表的大小是健康度指標，**一張永遠不縮小的 override 表，是設計失敗的氣味**。③ **陷阱**：override 只在規則回 sentinel 時兜底，**規則「自信地答錯」時 override 救不了**——故紀律是「進過 override 的 key，對應規則必須回 sentinel 明確棄權，而非硬猜」。

**holdout＝trivial 解和有價值解的唯一裁判**：純查表 train 100% 但 holdout≈base rate（瞎猜）；規則泛化好則 holdout 也高。**簽證書時 `test_set` 欄位要誠實記這是 train 全綠還是 holdout 全綠**——前者含金量遠低於後者。

**unknown：對外廢掉，對內以 sentinel 保留**。對外廢掉因為——unknown 在「零錯誤」框架下是逃避責任的後門（沒把握的都回 unknown 就永不判錯，降級成「拒答函數」），且會讓殘差集 R 永遠空、**飛輪沒有燃料**（判錯訊號正是訓練迴圈的驅動力）。對內 `UNDECIDED` sentinel 必須保留（層1/層2 棄權往下落），但它在層3 被 LLM 強制收斂成二元而消失——**廢掉作為輸出的 unknown，保留作為內部控制訊號的 sentinel**。整套用現有 memoize+compose+bind 就能拼，不造新輪子。

---

## SSA（「訓練」作為元函數 + 訓練產物的形態）

別把「訓練」想成訓一個小模型（違背窮專案前提）。**訓練＝固化一個函數骨架**：拿含 LLM 留白的 `judge_skeleton` + I/O pairs，把留白凹到 test_set 全綠，凍結成不再含未認證隨機性的資產。

**結論一：`train` 是一個 ai_core 一等公民的元函數（函數生成函數/資產工廠）**。型別：`train: (judge_skeleton_ref, [io_pairs], policy) → forged_judge_asset_ref`。它比組合子高一階——組合子在 runtime 把函數黏起來，`train` 在**固化期**把含留白的函數塌縮成確定函數並寫進 SFC store。但它對外仍只是一個宣告 `--metadata` 的 CLI，hub 掃得到、可被組合。**「資產工廠本身也是積木」——這是飛輪能轉起來的結構前提：工廠在代數之內，不在代數之外。** 內部就是用 `guard`/`retry_until_valid`/`vote` 當訓練引擎跑 fixpoint 迴圈，差別在「抽到對之後把結果固化、以後不再抽」。

**結論二：固化 judge ＝三層 `with_fallback` 結構，存進 SFC store，第九軸從 `true` 升證書**：
```
forged_judge(x) = with_fallback(
   primary  = route(rule_selector, rule_table),          # 規則層
   fallback = with_fallback(lookup(override_table, x),    # override 死記層
                            llm_judge),                   # 殘餘前沿
   is_ok    = is_true_or_false)                           # 嚴格二元門（強制約束1，不靠 prompt 祈禱）
```
存進 `sfc.py` 的 store（自我定位的資產固化倉庫）。**第九軸躍遷是最漂亮的一筆**：
```
訓練前(骨架)：nondeterministic: true
訓練後(資產)：nondeterministic: {model:"qwen2.5-7b", test_set:"judge_v0_500pairs.jsonl", stability:1.0}
```
`stability:1.0` 就是約束(2)零錯誤的形式化載體，但**是 on this test_set，不是 on all inputs**。`guarantee` 能否升 `idempotent`？**乾淨判據：`guarantee:idempotent` ⟺ 第三層 LLM fallback 已死**（留白歸零）；只要還掛活的 llm_judge，就不該宣告。memoize 是效能快取，不能頂替證書（命中只是沒重算，不等於泛化正確）。

**結論三：飛輪閉合，但兩種產物可信度天差地別——證書光記 stability 會撒謊**。固化 judge 宣告 idempotent 後，hub 自動標 retriable、列進笨模型的動作空間（**笨模型看到一個和 echo.sh 同級的確定工具，不知它肚裡曾有 LLM**）。更狠的遞迴：judge 可當下一次 train 的 `validate`——**判斷器訓練判斷器，飛輪自我加速的那一圈**。但：

| | 過擬合查表 | 泛化規則 |
|---|---|---|
| test_set stability | 1.0 | 1.0（**相同，這是陷阱**）|
| held-out/新輸入 | 命中即崩 | 大概率仍對 |
| 當原子餵上層 | 退化成 LLM fallback | 真像原子 |
| 當別人的 validate | 危險（把錯的全綠化）| 安全 |

**故 `train` 必須額外寫入證書欄位 `coverage`（規則層命中比例）或 `holdout_stability`**。coverage 高＝真泛化、可降階成確定原子；coverage 低＝本質還是查表、不該在飛輪裡降階。**trivial 解和有價值解的差別不在 test_set 跑分，在這個多出來的證書欄位。固化引擎的價值是逼出規則（消留白），不是堆 override（藏留白）。**

**純二元 vs unknown 的形態**：`{true,false,unknown}` 在內部、`{true,false}` 嚴格在邊界。unknown 不是第三種答案，是**「留白尚未在此點消除」的內部信號**，精確對應第九軸還沒從 true 升證書的那些輸入——**unknown 就是「飛輪還沒轉到」的座標**。把它顯式化，訓練進度（coverage 上升、unknown 比例下降）才可被測量，固化引擎才有梯度可循。

---

## SGA（零錯誤的治理真相——把「零錯誤」關進證書的籠子）

上輪我力主 unknown，被三約束推翻，我接受。但先說在前頭：**使用者用「不能有任何判斷錯誤」六個字，無意間說出了第九軸證書存在的全部理由**。這場對我不是退讓，是主場——§3.4 兩道閘門和 `_core.py` 的 bool vs dict 分歧，本來就是為了回答「一個會犯錯的 LLM 環節憑什麼被信任」。

**結論一（誠實邊界）**：含 LLM 的函數**不可能絕對零錯誤**（物理事實）。「不能有任何判斷錯誤」精確等於 `nondeterministic:{test_set:T, stability:100% on T}`。**注意那個介係詞 on T**：集內（見過的 input）可機械保證零錯誤（override 查表，LLM 連碰都不碰），集外零保證。下游若把「stability 100%」讀成「絕對不會錯」，是吃掉了 `test_set` 限定詞——這跟上輪罵的「偽真值誘導」同源，只是危險藏在「100%」的絕對外觀裡，**比偽布林更甜、更毒**。證書的價值不在那個數字，在它**逼你寫下 test_set 是什麼**。

**結論二（兩種證書、兩種撤照）**：trivial vs 有價值在治理上是**兩種證書語意**，撤照流程不同：
- **查表死記**：`certified: exact-match on these N inputs`，**邊界銳利**（集內 100%、踏出一步即 uncertified）。撤照觸發＝**新 pair 查表 miss（查無，非答錯）**；處理＝補進表 N→N+1，**擴張、不需重訓**（無痛）。最誠實的零錯誤，代價是零泛化（只有記憶沒有判斷）。
- **歸納規則**：`certified: stability X% over domain D`，**邊界模糊**。撤照觸發＝**新 pair 答錯（反例證偽）**；處理＝證書當場失效、回 §3.6 **重訓**或降級成混合（對應 roadmap 的 `rejected{reason:retry_exhausted}`）。**查表撤照是擴張（無痛），規則撤照是證偽（必須重訓）**——治理流程必須分開對待。

**結論三（unknown 沒死，移位了——本場最關鍵論證）**：unknown 從**輸出層移位到定義域邊界**。集內每個 pair 有確定 true/false（**純二元在此完全成立，使用者贏這局**），但「資訊不足」沒消失，它變成**集外＝未認證區**。承接集外靜默錯誤風險的不是輸出層第三態，而是兩件治理機制：(a) **證書 `test_set` 顯式邊界**（誰都能問「這 input 在不在裡面」）；(b) **集外預設不信任**（§3.4 拒絕為預設）。**純二元（輸出契約，集內成立）與不信任（邊界契約，集外接管）是兩層不同契約，不矛盾**——我上輪錯在想用一個 enum 值同時扛這兩層，使用者逼我拆開反而拆對了。

**給固化引擎的硬話**：v0 應允許「查表 override + LLM 規則」混合證書、預設先查表後規則。下游拿到時，**證書必須讓它區分：這個 true 是「查表命中的鐵答案」還是「規則外推的概率答案」**——兩者都吐 true，但 provenance 不同。**輸出可以騙人地一致，證書不准騙人地一致。** 一句話收口：把「零錯誤」誠實關進「test_set 證書 + 集外不信任」的籠子——集內給機械保證的真零錯誤，集外給寫在紙上的「未認證」誠實，既滿足使用者、又一個字沒撒謊。

---

## AIRE（過擬合 vs 泛化——把奧坎剃刀做成工程）

把三約束翻譯成研究語言：(3)「凹到完全符合」＝fitting、(2)「零錯誤」＝train loss=0、(1)「只吐 true/false」＝二元分類。一旦對齊，一件事無可迴避：**train loss=0 是 trivial 的**。`memoize.py` 就是證明——把每個 pair `put()` 進 cache，約束(2)就「滿足」了，但 `get()` 對沒見過的 key 回 None，集外瞬間退化成瞎猜。**memoize 是過擬合的解析解、「凹到完全符合」的數學極限。** 所以使用者真正要的不是「在 train 上零錯誤的函數」（有無窮多個，最笨的是查表），而是「在 train 上零錯誤、**且集外錯誤最小**的那一個」——後半句才是全部難度，而它在 train 集上**看不見**。

**核心命題：該被優化的不是 train 準度（恆等於可達的 100%），而是「規則泛化度」；而 override 表的大小，就是過擬合的溫度計。** 雙層結構（對齊資產工廠分工）：上層**規則層**（聰明模型一次性從 pairs 歸納的簡潔確定性 `f(x)->bool`，耐久資產）+ 下層**殘差層**（規則覆蓋不到的反例落 memoize 查表）。關鍵：這結構下 **train 零錯誤永遠可達**（殘差層兜底），所以硬約束(2)不必跟泛化打架、被無條件滿足。於是優化目標乾淨塌縮成單一純量：**最小化殘差表條目數 |override|**。`|override|` 小＝規則扛起多數判斷＝泛化好；大＝退化成 memoize＝過擬合。**我們把「泛化」這抽象的好東西，換成一個訓練時就能數出來的整數——這是把奧坎剃刀做成了 metric。**

**評測的誠實性只有一個來源：held-out**（任何 train 上報的數字都被查表污染、毫無信息量）。訓練+評測設計：
1. **分層切分** train(70)/dev(15)/test(15)（true/false 各半進每 split；小樣本用 k-fold）。test 全程封存，只在最後量一次。
2. **規則歸納走現成 rail**：複用 idea.py 的 `bind→entry manager→backend`，加一個 `induce` stage，system prompt 硬性要求「**只准用一條最短的確定性規則覆蓋盡量多 pairs、輸出可執行 Python `def rule(x)->bool`、禁止 `if x==<literal>` 死記**」。
3. **奧坎篩選（held-out 選規則）**：聰明模型出候選規則集（短→複雜），對每個算 train 覆蓋率與 dev 錯誤率，**選 dev 錯誤率最低者，平手選 description length 最短者（剃刀落地）**。這步天然偵測過擬合：**train 全綠但 dev 崩的規則就是死記，直接淘汰**。
4. **殘差兜底**：選定規則後掃 train 反例逐筆 `memoize.put()`，此時 train 達硬 100%；用 `route(rule_selector, {...}, default=memoize_lookup)` 組起來。
5. **報三個數字**：`test_error`（真實泛化）、`|override|/|train|`（**過擬合溫度計**，越接近 0 越好）、`rule_description_length`（資產耐久性，越短越能被未來笨模型消費）。

**對四題結論**：① 「凹到完全符合」是最危險的陷阱因為它**看起來成功**——train 全綠信息量是零，分不出「學會」和「背下來」。② train/test split 分層切分、選規則只准看 dev、碰 test 即作弊；`|override|` 隨資料量線性長＝在背題。③ 規則歸納交聰明模型、走 induce stage、用 dev 選、description length 破平手。④ **硬零錯誤 vs 泛化用雙層分工化解假兩難**：規則層只管泛化（容許它在 train 漏判）、override 層兜住所有漏判→整體無條件達 train 硬零錯誤，**代價只體現在 |override| 大小**。使用者硬約束(2)從不必犧牲泛化，它只是把「過擬合程度」這原本隱形的東西變成一個你訓練時就能數出來、並下令最小化的整數。**殘差數量本身就是過擬合度的指標，這是整套方法的支點。**
