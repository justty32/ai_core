# 事實判斷函數圓桌 Round 1：給敘述、僅吐是/否

> 簡單場景：用 stdlib 弄一個 `judge` 函數——給一段敘述，僅吐「是/否」。本質＝把不可靠 LLM 壓成嚴格 `text → {yes,no}` 介面。
> 主持人預設兩個分歧讓圓桌逼出：**(1) 二元 vs 三元（要不要 unknown）(2) 單次 vs vote**。四位發言由內部並行 subagent 獨立產出。

---

## SSE（最小可跑）

積木全有，不用新造輪子。`bind()` 把 LLM 包成 text→text（同 idea.py 的 `_stage_fn`），確定性 parser 是普通 Python，外套 `guard` 擋廢話、`retry_until_valid` 兜重抽——**唯一 LLM 留白只有「敘述→yes/no 這一刀語意轉換」**。關鍵設計：parser 做成「寬鬆吸收→嚴格壓平」，prompt 盡力逼單詞但**不信任**它，validator 才是真正的閘門。

```
judge(str) -> str（外層 CLI 再轉 exit code）：
  desc ─► [LLM留白] bind(system=JUDGE_SYS, suffix="只回 yes 或 no") ─► 自由文字
        ├─ guard(validate=is_clean, repair=parse)   合格放行；不合格 parse 硬抽壓平
        └─ retry_until_valid(validate=is_clean, retries=3, on_exhausted=→"unknown")

JUDGE_SYS：「你是事實判斷器。真只回 yes，假只回 no，無法判斷只回 unknown。
            禁止解釋/標點/句子。只准一個小寫單詞。」
parse(t)：strip().lower()；正則抽 yes/no/unknown/是/對/真/否/錯/假 → 壓成 {yes,no,unknown}；抽不到→unknown
is_clean(t)：t in {yes,no,unknown}            # guard 與 retry 共用同一把尺
emit(v)：print(v); exit({yes:0,no:1,unknown:2}[v])
```

guard 的修復函式**不用第二次 LLM**，用 parser 硬抽（「是的，因為月球較小」→抓開頭詞壓成 yes），抽不到才落 retry，用盡才 unknown。輸出協議走 **exit code 為主、stdout 為輔**（yes→0、no→1、unknown→2），讓 `judge "…" && echo 真` 直接能接。

**分歧表態**：(1) **要 unknown**——二元強迫表態會把「太陽是方的嗎」硬塞成 yes/no，是把噪音偽裝成信號；unknown 成本幾乎零，卻給 retry 用盡時誠實兜底（呼應「保證對或明確失敗」）。(2) **v0 單次，vote 緩做**——prompt 約束強、輸出空間只三個 token，單次配 guard+retry 方差夠低；`vote(n=5)` 是 5 倍成本換邊際穩定，對窮專案不划算。要時把 judge 整個塞進 `vote(judge, key=parse)` 即可（零改 parser），**vote 留作可隨時加掛的升級檔**。

---

## SSA（形態與組合性）

`judge` 跟現有積木咬合得極乾淨，幾乎不需要新東西。`classify` 在 `compose.route` 裡就是 `selector: str→str`（把輸入塌縮成有限 key），**`judge` 完全是這型別的真子集，枚舉只是 {yes,no}**。再立一個 judge 原子＝把「coreutils 粒度」換成「每個謂語一個執行檔」，是原子爆炸的開端。

**三題結論**：
1. **judge 是 classify 的特例（枚舉={yes,no}）**，stdlib 只該有**一個**泛用 classify，judge 至多是它的預設枚舉配置/alias，不另立原子。
2. **主協議用 stdout 印 `yes`/`no`**（餵 route selector、guard/retry 的 validate、vote 的計票 token）；**反對 exit code 當主協議**——route 的 `table.get(key)` 吃字串不吃 process 回傳碼，judge 只給 exit code 就接不進 route、破壞同構。exit code 0/1 當**從屬便利**供 shell `if`；JSON 信封僅在要帶 confidence/理由時才開（一個三句話二元函數不該逼下游解 JSON）。
3. 九軸：`lifecycle:one_shot`、`state:stateless`、`nondeterministic:true`（馴化框架觸發根）、**`guarantee:none`**——裸 judge 同一敘述兩次判斷不保證一致，**不能 memoize、不能當確定性積木**；要升 idempotent/可 memoize，路徑是用 `vote(judge,n)`/`retry` 包成穩定複合函數、跑測試組、才發證書。**穩定度是掙來的，不是宣告的。**

**分歧表態**：(1) **核心 judge 守二元 {yes,no}**——unknown 不是第三個答案，而是「該升 abstain」的訊號，交給 `classify --labels yes,no,unknown` 或在 guard/with_fallback 處理棄權，別把核心謂語污染成三態邏輯。(2) **裸函數單次**（保持原子最小、guarantee:none），要穩定性用 `vote(judge,n)` 組合子掙——這是 atom/combinator 代數的正確分工，不該把投票烤進原子。

---

## SGA（治理：小函數風險其實不小）

idea.py 那些子命令輸出**散文**（人類自然帶懷疑去讀）；本案 judge 吐的是 `yes`/`no`——一個**偽裝成布林真值**的 token。散文的不確定性寫在臉上，布林的不確定性卻被型別本身抹平了。**危險在於輸出的型別形狀誘導下游把機率當事實**：一個笨模型自信的錯 yes，餵進 route selector 或 gather single-truth 就成了 silent 錯誤傳播，而且因為「看起來確定」沒人會查。

我主張的不是「讓 judge 更準」，而是**讓 judge 不可能被當確定性事實消費**。三條護欄：(a) **第三態 unknown 必須是一等公民**——強迫笨模型資訊不足時二選一，等於框架**親手逼它幻覺**，直接違反「拒絕為預設」；abstain 是「拒絕為預設」在輸出層的鏡像。(b) **輸出不能是裸 yes**，必須攜帶溯源 `{verdict: yes|no|unknown, source:"llm", certified:false}`，讓「這是未認證 LLM 判斷」跟著值一起流動，下游無法假裝沒看見（元數據第九軸給 hub 看，但 runtime 回傳值要**自己再背一次**，兩層都標 guard 才能在資料流上攔）。(c) 下游硬規則：**未認證 verdict 進 single-truth 只能進待審 queue，不能直接落地**；當 if 條件時 unknown 必須走人類升級或保守 fallback，絕不被 truthy/falsy 默默吸收。

最關鍵也最省 LLM：**judge 預設先試確定性規則，只有真正語意模糊的才落 LLM**——「合法 email 嗎」「x>5 嗎」「JSON 有 key foo 嗎」用正則/比較/成員測試就有**確定性 yes**，不該召喚會幻覺的小模型來賭。架構上 judge 是 dispatcher：先過一層 deterministic predicate 註冊表，命中回 `{certified:true, source:"rule"}`（不帶第九軸）；查無對應、確屬開放語意判斷才落 LLM 軌回 `{certified:false}`。**必要性閘門（確定性程式碼辦不到才准 LLM 進場）內建進函數結構，而非靠人自律。**

**分歧表態**：二元 vs 三元——**堅定三元**，二元在治理上是缺陷不是簡潔。單次 vs vote——**反對預設 vote**：vote 用多次隨機平均**偽造**看似確定的數字（self-consistency≠correctness），反而更誘人當事實、且燒掉稀缺算力；與其投票買假信心，不如老實回 unknown 並升級。vote 至多作為**已領證書環節**的穩定度測量，不該當未認證 judge 的遮羞布。

---

## AIRE（可靠性，重數據）

**1. 單次方差很要命，vote 是事實判斷少數值得付 N 倍的場景。** 對模型沒把握的題，single-sample 逼近 50/50 雜訊、溫度>0 同題重抽會翻面。但這正是 self-consistency 最有效處：多數投票糾錯效力取決於「單次正確率 p>0.5」。p=0.7、N=5 多數正確率約 0.84、N=9 約 0.90。**關鍵不對稱：事實判斷 vote 便宜（短輸出，不像生碼跑整段）、二元「多數」定義零歧義**（compose.py `vote` 的 key 對 yes/no 正規化後直接 Counter.most_common）。建議 **N=5 預設、必為奇數避免平手**，難題升 9。注意 vote 對 p<0.5 的題會放大錯誤，它是「放大已有微弱信號」的工具，不是萬靈丹。

**2. 三元能用棄答換準度，對下游是淨贏。** 逼模型在資訊不足時二選一＝把它推進 p≈0.5 雜訊帶，50% 瞎猜錯誤計入準確率。給 unknown 出口後，模型在「敢答」的題上正確率分佈右移，**用棄答率換條件準度（conditional accuracy on answered）顯著提升**。關鍵：**unknown 是可路由的訊號，yes/no 錯卻是靜默污染**——下游可對 unknown 做 fallback（升級貴模型/補資訊/轉人工），但對自信講錯的 yes 無能為力。實作把 vote 和 abstain 疊起來：**vote 後眾數票數未達門檻（如 5 票裡 winner<4）就回 unknown，用「投票分歧度」當信心代理，零額外 LLM 成本。**

**3. 最省又最穩：直接答單詞 + 強制 parser，vote 取代 CoT，跳過 few-shot。** CoT 對二元準度幫助大但貴（長輸出×N=成本爆炸）、尾端 parser 抽結論又引入錯誤；few-shot 對「穩定吐單詞」有用但對「判斷正確性」幫助小、吃 context。**用「直接答單詞 + guard 強制格式」取代 CoT，把省下的成本投到 vote 的 N 上**——多次淺抽樣眾數在二元判斷上比單次深推理更穩更易解析。`bind(system="只回 YES/NO/UNKNOWN 其一，不要解釋", suffix="答案：")` + `retry_until_valid(validate=∈{YES,NO,UNKNOWN}, retries=3)`。few-shot 只在 validate 反覆失敗時加 1-2 例**格式**範例（非推理）。

**4. 量它比 raw 強：三軸對照。** raw（直接問、自己 parse）vs 鷹架（guard 強制單詞+可選 vote+可選 unknown）。固定一組有 ground-truth 的題（含一批「真的資訊不足」對照題測 abstain）。量：**(i) 條件準確率**＝答對/(答對+答錯)，排除棄答（鷹架主場）；**(ii) 棄答率**，搭 (i) 畫 accuracy-coverage 曲線；**(iii) 校準度**——用 vote 票數比當信心分，畫 reliability diagram/算 ECE，看「說很確定的題是不是真的更常對」（raw 無信心訊號、無法畫校準曲線，這本身就是鷹架的結構性勝利）。另量 tokens/題與呼叫次數讓「準度 vs N 倍成本」可取捨。

**分歧表態**：vote——**主張對事實判斷提前做、不要緩**（投報率最高場景：短輸出便宜、二元多數零歧義、順手產信心訊號餵 abstain；compose.py vote 已寫好、二元正規化 trivial）。二元 vs 三元——**主張三元**，純二元為「總是有答案」犧牲了下游最需要的「可路由的我不知道」訊號；棄答換準度在「便宜小模型+確定性下游」是明確正解，正是 roadmap「LLM 退縮、其餘交確定性程式碼」的微縮版。
