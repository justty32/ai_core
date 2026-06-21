# 事實判斷函數圓桌 Round 1：主持人收斂

> 把四份發言收斂成一份 `judge` v0 草案。這場有真分歧——重點在裁決三個分歧，並點出一個被誤認為對立、其實能變同盟的和解。

## 0. 一句話結論

`judge` 不是新原子，是 **`classify` 的一個預設枚舉配置**（labels = yes/no/unknown）。裸函數單次、`guarantee:none`、`nondeterministic:true`；要穩定性用 `vote` 組合子掙。**它表面是最簡單的函數，卻是整套治理哲學的最小完整縮影**：LLM 留白只有一刀（敘述→標籤），其餘全是確定性 parser + guard + retry；而它最大的風險不是會出錯，是「一個 `yes` 偽裝成布林真值、誘導下游把機率當事實」。

## 1. 已凍結的共識（四人無異）

| # | 共識 | 背書 |
|---|------|------|
| J1 | **judge 是 classify 的特例**（枚舉={yes,no[,unknown]}），不另立原子，stdlib 只留一個泛用 `classify`（避免原子爆炸） | SSA 提，全員不反對 |
| J2 | **唯一 LLM 留白是「敘述→標籤」一刀**；prompt 盡力逼單詞但**不信任**它，確定性 parser+guard 才是真正閘門 | SSE+AIRE |
| J3 | **裸 judge 單次、原子最小、`guarantee:none`**（同一敘述兩次不保證一致→不可 memoize、不可當確定性積木）；穩定度靠 vote/retry 包裝+測試組**掙**出證書 | SSA，AIRE 印證 |
| J4 | 九軸：`lifecycle:one_shot`、`state:stateless`、`nondeterministic:true`、`guarantee:none` | SSA |
| J5 | **prompt 配置：直接答單詞 + guard 強制格式**（不用 CoT——二元判斷下 CoT 貴又要抽結論）；few-shot 只在格式反覆失敗時加 1-2 例**格式**範例（非推理）；retry 硬上限 3 | AIRE |
| J6 | guard 的 repair **不用第二次 LLM**，用確定性 parser 硬抽（「是的，因為…」→抓詞壓成 yes），抽不到才 retry | SSE |

## 2. 三個分歧的裁決

### 分歧一：二元 vs 三元 → **三元，但用 SSA 的形態**

表面是分歧，其實**四人都要 unknown 出口**（SSE 要兜底、SGA 要誠實棄權、AIRE 要棄答換準度、SSA 也接受只是反對「三態污染核心」）。真正的問題只是**形態**。裁決採 SSA 方案，皆大歡喜：

- **unknown 不是「judge 內建的三態邏輯」，而是 `classify` 枚舉的一個 label**。`judge = classify --labels yes,no,unknown`。核心 classify 仍是「從固定枚舉選一個」的乾淨謂語，沒被污染；unknown 只是這次枚舉裡多一個值。
- 這同時滿足 SGA 的治理要求（abstain 是一等公民、是「拒絕為預設」在輸出層的鏡像）和 AIRE 的準度論證（棄答換條件準度）和 SSE 的工程兜底（retry 用盡→unknown）。
- **理由一句**：強迫笨模型在資訊不足時二選一，等於框架**親手逼它幻覺**；unknown 成本幾乎零，卻把「靜默污染的錯 yes」換成「可路由的我不知道」。

### 分歧二：單次 vs vote → **裸原子單次；vote 是組合子；且 vote 的真正價值是「產生 unknown 判據」而非「買假信心」**

這裡有個**被誤認為對立、其實是同盟**的和解，是這場最有價值的產出：

- **SGA 反對的**是 `vote-as-fake-certainty`：用多次隨機平均偽造一個看似確定的數字，誘導下游當事實（self-consistency≠correctness）。
- **AIRE 提的**其實是 `vote-as-confidence-signal`：**vote 後眾數票數未達門檻（5 票裡 winner<4）就回 unknown**，用「投票分歧度」當信心代理。

**這兩個是不同用法**。AIRE 的用法恰恰服務於 SGA 要的「誠實棄權」——投票分歧大→誠實回 unknown→下游路由 fallback，而不是 SGA 擔心的「投票抹平分歧→偽造一個自信的答案」。**點破這點，SGA 和 AIRE 從對立變同盟。** 裁決：

1. **裸 judge = 單次**（J3，四人共識）。vote 不烤進原子。
2. **vote 是組合子 `vote(judge, n)`，compose.py 已實作**——所以 SSE 的「v0 不增負擔」和 AIRE 的「提前可用」不衝突：不需新做，按需加掛。
3. **v0 提供一個 `judge` 的 voted-with-abstain 範例用法**：`vote(judge, n=5)` + 「winner 票數 < 門檻 → unknown」。N=5 預設、奇數避免平手。**這條 vote 的用途明確限定為「產生棄權判據」，不是「製造確定性」**——寫進文件，堵住 SGA 的顧慮。
4. vote 對 p<0.5 的題會放大錯誤（AIRE 警告），所以它不是預設、不是萬靈丹，是呼叫方在「該敘述值得多花 5×」時才疊的升級檔。

### 分歧三（SGA 獨特貢獻）：judge 該不該先試確定性規則 → **方向正確，但分層；v0 先純 LLM，dispatcher 是緊接的演進層**

SGA 主張 judge 是 dispatcher：先過確定性 predicate 註冊表（email 正則、x>5 比較、JSON has-key），命中回 `{certified:true, source:rule}`、不帶第九軸；查無才落 LLM。這把「必要性閘門」內建進結構，是 roadmap「LLM 退縮」的微縮體現——**完全正確的方向**。但對「簡單場景 v0」全套 dispatcher+規則庫會過重。分層裁決：

- **v0 的 judge ＝純 LLM 二元/三元判斷**（最小可跑，用戶要的「簡單」）。大多數真會來叫 judge 的本就是語意模糊的（確定性能做的根本不會來叫它）。
- **記為緊接的演進層**：`judge-dispatcher`（先試確定性規則、查無才落 LLM）。
- **回饋 stdlib 設計**：SGA 的洞察推出一個結論——**stdlib 該有一批確定性 predicate 原子**（`is-email` / `gt` / `has-key` / `matches`），它們是 `certified:true, source:rule` 的那一軌。judge 只負責「確定性 predicate 辦不到的語意模糊判斷」。這正是「真正需要 LLM 的只有語意轉換」在 predicate 層的落地。

## 3. 統一的 judge v0 設計

```
judge = classify --labels yes,no,unknown   （不是新原子）

裸函數（單次）：
  desc ─► [LLM留白] bind(system="只回 yes/no/unknown 其一，禁止解釋", suffix="答案：")
        ├─ guard(validate = ∈{yes,no,unknown}, repair = parse 硬抽)     # 確定性閘門
        └─ retry_until_valid(retries=3, on_exhausted → "unknown")        # 兜底，不拋例外

穩定升級檔（按需疊，非預設）：
  vote(judge, n=5)  + 「winner 票數 < 門檻 → unknown」                    # vote=產生棄權判據

九軸：lifecycle:one_shot · state:stateless · nondeterministic:true · guarantee:none
```

**兩層輸出協議**（調和 SSA vs SGA）：
- **預設（輕量）**：stdout 印小寫 `yes`/`no`/`unknown`（餵 route selector、guard/retry 的 validate、vote 計票）；exit code 0/1/2 當**從屬便利**供 shell `if`。**主協議是 stdout 字串**（SSA：exit code 接不進 route、會破壞同構）。
- **進真相層（當輸出要被當確定性事實消費：進 single-truth / 當 if 條件 / 餵 selector）**：強制升級成結構 `{verdict, source:"llm", certified:false}`（SGA），讓「未認證 LLM 判斷」跟著值流動。下游硬規則：**未認證 verdict 進 single-truth 只能進待審 queue、不可直接落地；unknown 當條件時走 fallback/人類升級，絕不被 truthy/falsy 默默吸收。**

## 4. 怎麼證明比 raw 強（AIRE，凍結為驗收方法）

raw（直接問、自己 parse）vs 鷹架（guard 強制單詞 + 可選 vote + unknown 出口）。固定一組有 ground-truth 的題（含一批「真的資訊不足」對照題測 abstain）。量：**(i) 條件準確率**（排除棄答，鷹架主場）、**(ii) 棄答率**（搭 i 畫 accuracy-coverage 曲線）、**(iii) 校準度**（vote 票數比當信心分，畫 reliability diagram / 算 ECE）。另量 tokens/題與呼叫次數。**raw 無信心訊號、畫不出校準曲線——這本身就是鷹架的結構性勝利。**

## 5. 回饋 stdlib 設計（這場逼出的東西）

這個「簡單」場景驗證並補強了 stdlib 圓桌的設計，三條要帶回去：
1. **classify 要支援 `--labels`**（judge 就是它的配置；unknown 是 label 不是特例邏輯）。
2. **stdlib 該有確定性 predicate 原子**（is-email/gt/has-key/matches）——judge-dispatcher 的確定性那一軌，落實「必要性閘門」。
3. **vote 的正當用途之一是「產生 abstain 判據」**（投票分歧→unknown），不只是穩定度提升——這給 vote 一個治理上正當的角色，回應「vote 緩做」時該保留的例外。

## 6. 下一步

- 這場與 stdlib/coding-agent 兩輪同屬「stdlib 應用範例」，可一起在開工時落成 `try_implement/` 的小 demo（judge 約 40 行，是最便宜的第一個可跑驗證）。
- 或繼續圓桌下一主題。

---
*產出方式：四份發言由內部並行 subagent 獨立扮演四位專家產生；收斂由主持人（Claude）撰寫。本輪未動 `_core.py` / `core_nature/`。*
