# 固化引擎圓桌 Round 1：主持人收斂

> 把四份發言收斂成 ai_core 固化引擎的定案草案。這場回答了 roadmap §3.6「這題優先」的最硬未決題——**手動 vs 自動**。

## 0. 一句話結論

**固化引擎的「換規則」部分已被 judge 訓練那輪解掉了**（三層三明治 + `with_fallback`，零新組合子）。**真正的難題是兩件事：(1) 時機——一個案例到底成熟到可以固化了沒；(2) 治理——固化是唯一「主動擴張確定性領地」的動作，錯了 silent + 永久 + 監督已退場。** roadmap §3.6 的答案因此精確化為：**證據生產可全自動，領地擴張需蓋章；v0 先全手動，但現在就補 trace 的 I/O 原料（不可後補）。**

## 1. 四人零分歧的共識

| # | 共識 | 背書 |
|---|------|------|
| K1 | **「換規則」已解**：固化把 case 從 LLM fallback→override→規則層遷移，機制＝`with_fallback(新規則, 原 LLM)`，遞增疊加，無需新組合子 | 四人（承 judge round2）|
| K2 | **真正的難題是「就緒度判斷」**：太早固化＝把飛輪免費能量換成 silent 脆規則（train 100%、holdout 悄悄崩、無 runtime 報錯）| AIRE 主場，SSE/SGA 呼應 |
| K3 | **固化閘門掛在 coverage/holdout，不掛在 stability**：stability 在 test_set 100% 是零信息量（查表與真規則都全綠）；低 coverage＝藏起的查表，不准降階 | SGA+AIRE 完全一致 |
| K4 | **固化必須可逆**：fallback 層**休眠不刪**（保留影子分支）、固化分支帶反例計數器、撤照＝路由切回不刪資產。`guarantee:idempotent ⟺ LLM 層已死`，故 v0 寧可不讓 fallback 真死 | SGA+SSA 完全一致 |
| K5 | **固化引擎不是 monolith，是三段 chain**：`scan(生態)→候選` ∘ `prioritize(trace頻率)→排程` ∘ `train(目標,pairs)→資產`，三者都是普通 ai_core 函數 | SSA |
| K6 | **半自動的精確劃線**：證據生產（掃 log/歸納候選/跑 holdout/算 coverage/填證書草稿）可全自動；**領地擴張（把規則設為預設、讓 LLM 退場）需人類/聰明模型蓋章** | SGA，全員認同 |
| K7 | **v0 先全手動，只證明洞填得準**——但**現在就補 trace 的 I/O 欄位**讓原料開始累積（廉價且不可後補：歷史 trace 丟了就丟了）| SSE，全員不反對 |

## 2. 固化引擎的形態（SSA）+ 元件分工

**固化引擎 = 三段 chain，非 monolith**（拆開後大部分難度蒸發，避免「設計一個無所不能的大引擎」違反 KISS）：
```
scan(生態) → 候選清單  ∘  prioritize(候選, trace頻率) → 排程  ∘  train(目標, pairs, policy) → forged 資產
```
- **train** = 對單一目標的固化元函數（judge round2 已定義，收在代數內、帶 --metadata）
- **固化引擎** = 比 train 高一階的編排器

**元件分工固定**：
| 角色 | 元件 | 職責 |
|------|------|------|
| 原料 | **trace** | 每次 LLM 留白命中＝一條 span，accumulate＝I/O pairs 礦 |
| 產物倉庫 | **SFC store** | forged judge ＝一個 tiny function（compile_function 直接載入）|
| 生態掃描器 | **hub 升級版** | 讀第九軸把 `nondeterministic:true` 標成固化候選 + 交叉 trace 頻率排序 |
| 證書帳本 | **第九軸 metadata** | `true → {model,test_set,stability,coverage,holdout,forged_from}` |
| 索引 | **indexer（不變）** | 純 --metadata 彙整 |

## 3. 真正的難題①：就緒度判斷（AIRE 的 `ready()`）

**就緒度 = 四道 AND 閘（不是 OR）**，把「拍腦袋判斷」變成可寫成函式的確定性判官：
```
ready = (n ≥ N_min)        # 樣本量閘（v0=30，低於此連 holdout 都切不出＝三點擬合）
      ∧ (stability ≥ θ)    # 穩定度閘（同 input 一致率≥0.95；不穩＝本質模糊，一票否決）
      ∧ (coverage ≥ c_min) # coverage 閘（規則命中比例，低＝藏起的查表，測謊器）
      ∧ holdout_ok         # holdout 閘（封存 trace stability 不顯著低於 train，唯一裁判）
```
**`ready()` 本身可被 hub 掃描、被另一個 train 當 validate（判斷器判斷判斷器）。**

**固化 vs 永遠留 LLM 的判據**：該固化＝`高頻 ∧ 穩定 ∧ 規則簡潔`；該留 LLM＝`低頻 ∨ 不穩 ∨ 規則必然複雜`。**固化一個本質模糊的東西＝用脆規則假裝確定，比誠實留給 LLM 更糟。留白不是失敗，是對「這裡確實模糊」的誠實宣告。**

**量固化成效（A/B 對照，淨贏判據）**：`固化後 holdout ≥ 固化前 LLM holdout − ε  且  save>0  且  coverage 合理`。**holdout 準確率是一票否決的脆化偵測器——規則 holdout 顯著低於 LLM holdout 就回滾，省再多錢都不算贏。**

## 4. 真正的難題②：治理三重籠子（SGA）

**地基——錯誤固化的傷害嚴格高於 LLM 犯錯**：LLM 犯錯是「可見、可重試、被懷疑」；固化犯錯是「silent（確定性程式碼自帶可信外觀）、不重試（沒有下一輪）、被信任、且發生在監督退場後（LLM 退場＝盯著它的智能也退場）」。**固化是這套系統裡唯一同時 silent+永久+監督退場後發生的傷害。** 故固化預設姿態＝**拒絕**（與 LLM 進場同預設、反向）。

三重籠子：
1. **閘門**：掛 coverage + holdout（不掛 stability，後者零信息量）；證書四欄完整，缺 coverage 視為偽造照拒收。
2. **可逆**：fallback 休眠不刪 + 反例計數器 + 兩種撤照分流（查表 miss＝無痛擴張可線上 / 規則答錯＝**證偽 incident**，意味 coverage 證書撒謊、騙過 holdout，必須跳閘降級回 fallback、證書標待重認證）。
3. **蓋章**：證據生產可全自動；**領地擴張需授權**。v0 永遠需蓋章不准全自動：(a) 物理刪除 fallback 的固化；(b) coverage 低於門檻仍想固化（查表偽裝成規則）；(c) 同一座標二次以上重固化（有撤照前科）。唯一可全自動：override 表的無痛擴張。

## 5. 資產 vs 證書：單向 + 雙向（SSA+SGA 一致）

- **資產（規則/override）單向、只進不退、永久累積**——刪了等於丟棄聰明模型一次性付的成本。
- **證書雙向狀態機**：`true ⇄ {證書}`。撤照＝狀態回退（`guarantee:idempotent→none`、`nondeterministic:證書→true`、旁路重開 LLM fallback 層），**不刪資產**。
- 第九軸 dict 加 `forged_from`/`asset_ref` 指回三明治骨架，讓「撤照的撤照」無損可逆（重新認證即回去）。

## 6. 原料缺口（SSE，v0 唯一現在就該動的固化動作）

- **trace.py 現在只記拓撲與時序，不記 payload**——固化原料（I/O pairs）根本沒在累積。
- **補法**：LLM 環節的 span 用既有 `**fields` 記 `io={"in","out"}`（大 payload 存 digest+旁路檔）。**廉價、且不可後補**（歷史 trace 丟了就丟了）。
- **judge pairs vs 野生 trace 的裂縫**：judge 的原料是「人背書的正解」，trace 的原料是「LLM 當時這樣答、對錯未知」。故通用固化必須比 judge 多一道**正解來源/驗證**設計（不能假裝兩者一樣）。

## 7. 遞迴閉合，但 v0 踩剎車（SSA）

固化引擎是 ai_core 函數、遞迴閉合（hub 掃 hub、train 能固化 train 內部的 judge）。**但 v0 對「准入閘門判斷」保持 `nondeterministic:true` 不自我固化**——固化它自己＝把准入閘門交給機器，而治理要求閘門要慢、可審計。**讓飛輪轉，但別讓飛輪自己決定要不要拆掉自己的剎車。**

## 8. 對 roadmap §3.6 的答案 + 規範增補

- **§3.6「手動 vs 自動」的答案＝分層**：**證據生產可自動，領地擴張需蓋章**；半自動的「半」在「就緒度門檻 + 蓋章」，不在規則生成。**v0 先全手動 + 補 trace io 原料**，等 trace 累積到人手跟不上、`|override|` 溫度計發燙，再把 scan/提案自動化。
- **規範增補建議**（累計）：
  1. **trace schema 加 `io` 欄位約定**（LLM span 記 input/output）——v0 就該做。
  2. **第九軸證書加 `coverage` / `holdout` / `forged_from` 欄位**（承 judge round2 的 coverage 建議再擴）。
  3. **`ready()` 就緒度判據**（四道 AND 閘）可作為固化引擎的核心契約。
  待定奪是否扶正進 `core_nature/`。

## 9. 下一步

- 這場與 judge 訓練那輪共同構成「固化」的完整設計：judge round2＝怎麼固化（三明治），本場＝何時固化（ready 四閘）+ 怎麼治理（三重籠子）+ 系統形態（三段 chain）。
- **v0 可落地的一小步**：trace 加 io 欄位（純標準庫、廉價、不可後補）——是固化的原料地基，建議與 ATP v0 一起做。
- 或繼續圓桌下一主題。

---
*產出方式：四份發言由內部並行 subagent 獨立扮演四位專家產生；收斂由主持人（Claude）撰寫。本輪未動 `_core.py` / `core_nature/`，提出 trace `io` 欄位 + 第九軸 coverage/holdout/forged_from + ready() 判據 的增補建議。*
