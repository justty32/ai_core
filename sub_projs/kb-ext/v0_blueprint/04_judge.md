# 04 · 事實判斷函數（judge）

> 定案＝給一段敘述、僅吐是/否的函數。過程：`discussion_logs/judge_round_1_*`。**尚未登記進 roadmap/DECISIONS。**
> 註：本檔是 judge 的「單次判斷」版；使用者後續補充「零錯誤 + 訓練」見 [05_judge_training.md](05_judge_training.md)。

## 一句話

`judge` **不是新原子，是 `classify` 的一個預設枚舉配置**（labels = yes/no/unknown）。表面最簡單，卻是整套治理哲學的最小縮影——LLM 留白只有一刀（敘述→標籤），最大風險不是會出錯，是「一個 `yes` 偽裝成布林真值、誘導下游把機率當事實」。

## 設計

```
desc → [LLM留白] bind(system="只回 yes/no/unknown，禁止解釋", suffix="答案：")
     ├─ guard(validate = ∈{yes,no,unknown}, repair = parse 硬抽)   # 確定性閘門
     └─ retry_until_valid(retries=3, on_exhausted → "unknown")
穩定升級檔（按需疊）：vote(judge, n=5) + 「winner 票數<門檻 → unknown」
九軸：one_shot · stateless · nondeterministic:true · guarantee:none
```
guard 的 repair **不用第二次 LLM**，用確定性 parser 硬抽。

## 三個裁決

1. **二元 vs 三元 → 三元**：unknown 不是「三態邏輯污染原子」，是 classify 枚舉的一個 label。強迫資訊不足時二選一＝框架親手逼它幻覺。
2. **單次 vs vote → 裸原子單次；vote 是組合子**。和解：SGA 反對 vote「偽造確定性」，AIRE 提的是 vote 票數**分歧度→觸發 unknown**——這恰恰服務於誠實棄權。**點破這點，SGA 和 AIRE 從對立變同盟。**
3. **judge 先試確定性規則 → 方向正確、分層**：v0 先純 LLM，記為演進層 judge-dispatcher；逼出 stdlib 結論——該有確定性 predicate 原子（is-email/gt/has-key），judge 只處理確定性辦不到的語意模糊判斷。

## 兩層輸出協議

- 預設（輕量）：stdout 印 `yes`/`no`/`unknown`（餵 route selector、guard validate、vote 計票）；exit code 0/1/2 從屬便利。
- 進真相層（當輸出被當確定性事實消費）：升級成 `{verdict, source:"llm", certified:false}`，未認證 verdict 進 single-truth 只能進待審 queue。

## 回饋 stdlib

① classify 要支援 `--labels` ② stdlib 該有確定性 predicate 原子 ③ vote 的正當用途之一是「產生 abstain 判據」。
