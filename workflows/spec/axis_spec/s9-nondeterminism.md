## §9 確定性 / 隨機性（nondeterminism）

> `execution_forms.md §0` 原本只列了八軸。本節是後續**新增**的第九軸。新增的理由與它和治理
> 原則的關係，記在這裡；metadata field 的格式定義在 [`lib_spec.md §9`](../lib_spec/README.md)。

### Phase 1 分析

前八軸有一個**隱含的共同前提**：函式是確定性的（deterministic）——同樣的輸入，跑幾次都得到
同樣的輸出。八軸描述的是「怎麼跑、跑多久、有無副作用、能否中斷」這些**執行特性**，沒有一軸
描述「輸出本身可不可預測」。

但 ai_core 的存在理由之一，正是要駕馭一個**天生隨機**的函式：LLM。把 LLM 視為一個函式時，它
打破了那個隱含前提——同樣的 prompt，兩次呼叫可能給不同答案。這個性質：

- **無法由任何既有軸隱含**。`lifecycle` / `state` / `interruptible` / `guarantee` / `resources`
  全都描述不了「隨機」。（對比：中斷恢復能力可由 `interruptible` / `guarantee` 隱含，所以那條
  複合慣例選擇「不新增欄位」。隨機性沒有這種隱含來源。）
- **與其他軸正交**。隨機 ≠ 有狀態，也 ≠ 無保證；一個函式可以同時是隨機的、有狀態的、無保證的。

因此「確定性 / 隨機性」必須成為一個**獨立的、明確宣告的軸**。

### 這個軸承載「治理原則」

這不只是一個布林標記。它是整套治理原則（見 `roadmap.md §3.4`「拒絕為預設、憑證准入」）能否
落地的**錨點**：

- **拒絕為預設**：函式預設是確定性的（`nondeterministic` 缺席）。要讓一個環節變成「由 LLM 填」，
  得**主動宣告** `nondeterministic`——舉證責任在「讓 LLM 進來」那邊。
- **兩種強度＝兩個 regime**：
  - `nondeterministic: true`（**開機期**）：只標記隨機，未認證。允許存在，但帶著「待認證」的技術債。
  - `nondeterministic: {model, test_set, stability}`（**成熟期**）：證書。LLM 不是被信任，是被
    **認證**；附「用哪個模型、經哪個測試組、穩定度幾 %」，因此**可稽核、可撤照**（哪天有人想出
    確定性解法，就把這個 LLM 環節踢掉、移除宣告）。
- **飛輪＝從 `true` 遷往 `{證書}`，再遷往「移除宣告」**：把 LLM 環節換成確定性碼（撤照）、或給
  活下來的環節發證書（認證）。這個軸讓「系統現在有幾個隨機點、各自多穩、用什麼擋著」變成
  可查詢的事實。

### 與馴化框架 / 複合慣例的關係（未來工作）

`nondeterministic: true` 是 LLM 馴化框架（`sub_projs/ver_1/try_implement/docs/llm_taming_framework.md`：契約 /
確定化 / 驗證 / 聚合 / 編排五層）的**觸發根**：一個函式宣告自己隨機，才有「需要被馴化」的問題。
馴化的零件（retry_until_valid / vote / best_of / guard …）已在 `sub_projs/ver_1/try_implement/lib/compose.py`
原型化，其作用正是把一個 `nondeterministic: true` 的環節，收斂到「穩定到足以發 `{stability: …}`
證書」。

> **待議（複合慣例候選）**：是否把「nondeterministic 環節 + 馴化組合子 + 證書簽發」收斂成一條
> 具名的**複合規範**（類比「中斷恢復慣例」），定義證書由誰簽發、撤照流程、stability 怎麼量。
> 這條會直接連到 `roadmap.md §6` 的 v0 切片——等 v0 真的跑出一個「笨模型 + 鷹架」的可靠編輯，
> 證書該長什麼樣就會被逼出來。在那之前，本軸先把「宣告隨機 + 可帶證書」這個最小骨架立好。

### 相關的非軸欄位：`reliability`（2026-07-03 新增）

在完整憑證機制落地前，meta 先提供一個**可變動的可靠度數值欄位** `reliability`（0.0–1.0，
可帶 provenance 的物件形式）。它**不是軸**——軸是靜態自述，它是隨觀測更新的量測值——但與本軸
分工明確：證書的 `stability` 是認證當下的凍結值，`reliability` 是當前的活值，也是日後簽發 /
撤照的數據來源。格式定義見 [`lib_spec.md`「非軸欄位：reliability」](../lib_spec/README.md)。
