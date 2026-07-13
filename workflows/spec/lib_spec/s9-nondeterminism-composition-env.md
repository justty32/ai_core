## §9 確定性 / 隨機性（nondeterminism）

> 這是繼 §1–§6 之後**新增**的軸。前八軸描述的都是「執行特性」（怎麼跑、有無狀態、能否中斷…），
> 預設假設函式是**確定性**的（同輸入 → 同輸出）。但 ai_core 的核心對象之一是 LLM——一個天生
> **隨機**的函式。這個軸把「此環節是隨機的」標出來，並承載治理原則所需的**證書**。詳細的軸層
> 討論見 [`axis_spec.md` §9](../axis_spec/README.md)。

### metadata field

| key | 型別 | 說明 |
|---|---|---|
| `nondeterministic` | bool \| object | 宣告此函式（或此調用環節）含不可確定性 |

**預設值**：若 `nondeterministic` 缺席、為 `null` 或 `false` → 視為**確定性**函式（同輸入同輸出）。
只有明確含隨機環節（典型是包了 LLM 的函式）時才宣告。

### 說明

| 形式 | 語意 |
|---|---|
| `nondeterministic: true` | **未認證的隨機環節**。只標記「這裡是隨機的」，是 LLM 馴化框架的觸發根，但尚未經測試認證。對應治理的「開機期」——允許存在，但帶著「待認證」的債。 |
| `nondeterministic: {…}` | **證書**。不是含糊的「這裡隨機」，而是「**此環節用模型 X、經測試組 Y、認證穩定度 Z%**」。對應治理的「成熟期」——LLM 不是被信任，是被**認證**，且可被**撤照**。 |

### 證書（object 形式）的建議欄位

沿用 §4 `resources` 的設計——**自由 key-value，預定義 key 有建議語意，自訂 key 不受限**。
validation 只確保它是 dict，不強制特定 key（規範「從粗糙到嚴整」，待 v0 驗證後再收緊）。

| 建議 key | 語意 |
|---|---|
| `model` | 此環節用哪個模型填（如 `"local-8b"`、`"claude-opus"`） |
| `test_set` | 認證所依據的測試組識別 |
| `stability` | 認證穩定度（如 `"92%"` 或 `0.92`）——證書的核心主張 |

### 為何是新軸，而非塞進既有軸

`nondeterministic` 與 `memoized`（見下「未入軸的決策」）共有一個處境：**沒有任何既有軸值可以
隱含它**。中斷恢復慣例之所以「不新增欄位」，是因為它能由 `interruptible` / `guarantee` 隱含觸發；
但「這函式是不是隨機的」在八軸裡完全無從推斷——`lifecycle` / `state` / `guarantee` 都描述不了。
因此它**必須**是一個獨立的宣告。

### 與其他軸的關係（正交）

`nondeterministic` 與所有執行特性軸正交：一個函式可以同時是 `nondeterministic: true` 且
`state: "stateful_external"` 且 `guarantee: "none"`。隨機性是「輸出由何決定」的性質，與「有無副作用」
「能否重試」彼此獨立。

> **與 §6 `guarantee` 的對照**：`guarantee` 講的是「對外部狀態的承諾」；`nondeterministic`
> 講的是「輸出本身可不可預測」。一個 LLM 包裝函式常是 `guarantee: "none"` + `nondeterministic: true`——
> 既不保證副作用冪等，輸出也不可預測。馴化框架（retry / vote / guard …，見 `sub_projs/ver_1/try_implement/docs/`）
> 正是把後者收斂成「夠穩到能發證書」的機器。

### register() 範例

```python
# 開機期：標記隨機、尚未認證
ai_core.register(nondeterministic=True)

# 成熟期：帶證書
ai_core.register(nondeterministic={
    "model": "local-8b",
    "test_set": "code_qa_v1",
    "stability": "92%",
})
```

`--metadata` 輸出：

```json
{"nondeterministic": {"model": "local-8b", "test_set": "code_qa_v1", "stability": "92%"}}
```

---

## 非軸欄位：`reliability`（可靠度數值）

> 2026-07-03 新增，落實「回到初心」逐點決斷的近期焦點②（`workflows/notes/20260702-2003-回到初心-llm-as-function.md`
> §12.1）：完整憑證機制押後，**目前的最低要求＝meta 輸出一個「可靠度」數值、可變動**。

### metadata field

| key | 型別 | 說明 |
|---|---|---|
| `reliability` | number 或 object | 此函式當前的可靠度估計，`0.0`–`1.0` |

**數值形式**：

```python
ai_core.register(nondeterministic=True, reliability=0.92)
```

**物件形式（帶 provenance）**——必含 `value`（0.0–1.0），其餘 key 自由；建議：
`measured_on`（在哪個測試組 / 流量上量的）、`n`（觀測數）、`window`（時窗）：

```json
"reliability": {"value": 0.92, "measured_on": "code_qa_v1", "n": 130}
```

### 為何是「非軸」

九軸是執行特性的**靜態自述**——函式怎麼跑、有無副作用、能否中斷，寫定即不變。
`reliability` 性質不同：它是**隨觀測更新的量測值**（每次呼叫的成敗都可能改變它），
同一函式的 reliability 會隨時間、隨所用模型變動。它放進 meta 是為了讓排程器 / 呼叫方
讀得到，但語意上是「當前狀態的快照」而非「本性宣告」，故不列入軸。

### 與第九軸證書的分工

- 第九軸證書的 `stability` ＝ **認證當下的凍結值**（審批紀錄的一部分，改它＝重新認證）。
- `reliability` ＝ **當前的活值**（隨每次觀測更新的後驗估計）。
- 飛輪視角：`reliability` 的持續量測正是日後簽發 / 撤銷證書的數據來源；排程器做
  「成本 × 可靠度」拍賣時吃的是這個活值。deterministic 函式也可以有 reliability
  （確定性 ≠ 不會失敗——網路呼叫、外部相依都會敗）。

> provenance 為何重要：沒有「在什麼上量的」的可靠度是無條件機率，排程器吃了等於垃圾進
> 垃圾出（§13.2 推論一）。數值形式仍允許，作為最低門檻；但建議盡早升級為物件形式。

---

## 未入軸的決策：`memoized`（純 runtime）

`memoized`（記憶化 / 輸入→輸出快取）曾與 `nondeterministic` 並列為候選新軸。**決策：不入
metadata 軸，維持純 runtime 行為**（由 `sub_projs/ver_1/try_implement/lib/memoize.py` 這類 library 在執行期處理）。

理由：快取是**呼叫方 / library 的優化決策**，不是函式對外的語意承諾。一個函式「可不可以被快取」
其實由既有軸隱含——`nondeterministic` 缺席（確定性）＋ `state: "stateless"` 的函式天然可安全
memoize；隨機或有狀態的函式則否。因此 memoize 不像 `nondeterministic` 那樣「無既有軸值可隱含」，
不需要新欄位。這與「中斷恢復慣例不新增欄位」同精神：能由既有軸推斷的，就不膨脹軸層。

> 若日後出現「函式想主動宣告自己的快取版本 / TTL 語意」的真實需求（而非由呼叫方決定），再
> 重新評估是否補欄位。目前 `lib/memoize.py` 的 cache key / 失效策略由呼叫方明確指定，足夠。

---

## §7 組合模式

**此軸不在本規範範圍內，不定義 metadata fields。**

組合模式描述的是呼叫鏈（call chain）的結構：fan-out/fan-in、proxy/wrapper、hook/callback、agentic loop 等。這是呼叫方（hub、router）的關切點，不是單一執行單元的屬性——程式本身不需要、也不應該知道自己被如何組合。

> 若日後需要描述呼叫鏈拓撲，應另立文件（如 `call_graph_spec.md`），而非在單一程式的 `--metadata` 中宣告。

---

## §8 環境模式

**此軸不在本規範範圍內，不定義 metadata fields。**

程式本身不管理自己的執行環境。本規範的預設假設為**穩定的 Linux 環境**；容器化、venv、resource limit 等屬於部署/執行層的選擇，不影響程式介面設計。

唯一例外是**包裝程式（wrapper）**——它的職責是修改子程式的執行環境（設置環境變數、切換工作目錄、套用 resource limit 等）。這個包裝程式本身是一個執行單元，其行為用其他軸描述；環境管理只是它的輸出副作用，不需要額外的 metadata field。
