# 06 · 固化引擎（Crystallization Engine）

> 定案＝roadmap §3.6「這題優先」的最硬未決題（手動 vs 自動）的通用形態。過程：`discussion_logs/crystallization_round_1_*`。**尚未登記進 roadmap/DECISIONS。**
> 與 [05_judge_training.md](05_judge_training.md) 共同構成「固化」的完整設計：05＝**怎麼固化**（三明治），本檔＝**何時固化 + 怎麼治理 + 系統形態**。

## 一句話

固化＝飛輪的**撤照支**（把 LLM 環節換成確定性程式碼）。**「換規則」已被 05 解掉**（`with_fallback(新規則, 原 LLM)`，遞增疊加、零新組合子）。真正的難題剩兩件：**(1) 何時固化（時機/就緒度）(2) 怎麼治理（固化是唯一主動擴張確定性領地的動作，錯了 silent+永久+監督已退場）**。

## 難題① 就緒度判斷：`ready()` 四道 AND 閘（AIRE）

```
ready = (n ≥ N_min=30)      # 樣本量：低於此連 holdout 都切不出＝三點擬合
      ∧ (stability ≥ θ=0.95)# 穩定度：同 input 一致率；不穩＝本質模糊，一票否決
      ∧ (coverage ≥ c_min)  # 規則命中比例；低＝藏起的查表，測謊器
      ∧ holdout_ok          # 封存 trace stability 不顯著低於 train，唯一裁判
```
`ready()` 本身可被 hub 掃描、被另一個 train 當 validate。
**固化 vs 留 LLM**：固化＝`高頻∧穩定∧規則簡潔`；留 LLM＝`低頻∨不穩∨規則必然複雜`。**固化本質模糊的東西＝用脆規則假裝確定，比誠實留 LLM 更糟。**
**淨贏判據**：`固化後 holdout ≥ 固化前 LLM holdout − ε 且 save>0 且 coverage 合理`。holdout 降就回滾，省再多錢都不算贏。

## 難題② 治理三重籠子（SGA）

**地基**：錯誤固化傷害嚴格高於 LLM 犯錯——silent（確定性碼自帶可信外觀）+ 不重試（沒有下一輪）+ 監督退場後（LLM 退場＝盯它的智能也退場）。固化預設姿態＝**拒絕**。
1. **閘門**：掛 **coverage + holdout，不掛 stability**（後者在 test_set 100% 是零信息量）；證書缺 coverage 視為偽造照拒收。
2. **可逆**：fallback 層**休眠不刪**（影子分支）+ 反例計數器 + 兩種撤照（查表 miss＝無痛擴張 / 規則答錯＝證偽 incident、跳閘降級）。
3. **蓋章**：**證據生產可全自動，領地擴張需授權**。唯一可全自動：override 表無痛擴張。

## 系統形態：三段 chain，非 monolith（SSA）

```
scan(生態)→候選 ∘ prioritize(trace頻率)→排程 ∘ train(目標,pairs)→forged 資產
```
元件分工：**trace＝原料**（I/O pairs 礦）· **SFC store＝產物倉庫** · **hub 升級版＝掃描器**（挑 nondeterministic:true + trace 頻率排序）· **indexer 不變** · **第九軸＝證書帳本**。
**資產單向（只進不退、永久累積）、證書雙向**（`true ⇄ {證書}`，撤照＝旁路重開 LLM 層不刪資產；第九軸加 `forged_from` 指回骨架）。
遞迴閉合（hub 掃 hub、train 固化 train 內部 judge），**但 v0 對「准入閘門判斷」不自我固化**——別讓飛輪自己拆自己的剎車。

## v0 立場與 §3.6 的答案

**§3.6「手動 vs 自動」＝分層**：證據生產可自動、領地擴張需蓋章；半自動的「半」在就緒度門檻 + 蓋章，不在規則生成。
**v0 先全手動**（v0 切片裡還沒有需要固化的對象，trace 是空的），**但現在就補 trace 的 `io` 欄位**讓原料開始累積——**廉價且不可後補**（歷史 trace 丟了就丟了）。這是 v0 唯一現在該動的固化動作，建議與 ATP v0 一起做。

## 規範增補建議（累計）

1. **trace schema 加 `io` 欄位**（LLM span 記 input/output）— v0 就該做。
2. **第九軸證書加 `coverage`/`holdout`/`forged_from`** 欄位。
3. **`ready()` 四道 AND 閘**可作固化引擎核心契約。

待定奪是否扶正進 `core_nature/`。
