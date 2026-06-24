# 軸 9：`nondeterministic`（確定性 / 隨機性・治理證書）

> 狀態：✅ 定案（Round 1）。單一 `unsigned uncertainty = 0`（0＝完全確定；愈高愈不確定；馴化使其下降）+ `extra`。
> 九軸的靈魂軸：治理原則「拒絕為預設＋憑證准入」的資料載體。**九軸描述面到此收尾。**

---

## 0. 是什麼

前八軸預設函式是**確定性**的（同輸入→同輸出）。但 ai_core 的核心對象之一是 LLM——天生**隨機**。
本軸把「此環節的不確定性」標出來，並承載治理所需的**證書**。

治理原則：LLM 預設零領地（**拒絕為預設**），要佔一環節須通過「必要性＋穩定性」兩道閘門、領一張
**證書**（用哪個模型、測試組、穩定度 %），可被**撤照**。本軸就是這張證書在 metadata 裡的形狀。

---

## 1. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`_validate_nondeterministic`：`bool | dict`）、`core_nature/lib_spec.md §9`、
`core_nature/axis_spec.md §9`。權威是**三態**：

| 權威形式 | 語意 | 治理階段 |
|---|---|---|
| 缺席／`false` | **確定性**函式。**預設** | — |
| `true` | **未認證的隨機**：標記「這裡隨機」，馴化框架觸發根，帶「待認證」的債 | 開機期 |
| `{model, test_set, stability}` | **證書**：用模型 X、經測試組 Y、認證穩定度 Z%。可撤照 | 成熟期 |

- 證書 object 沿用 §4 resources：自由 key-value，預定義 key（`model`/`test_set`/`stability`）有建議語意、
  自訂不限；validation 只確保是 dict（「從粗糙到嚴整」，待 v0 收緊）。
- 與所有軸正交。與軸 7 對照：guarantee 講「對外部狀態的承諾」，本軸講「輸出本身可不可預測」。

---

## 2. Round 1（✅ 拍板）：摺成單一不確定性量值

**不採**我原提案的 `optional<Certificate>` 三態 presence。改用**單一 `unsigned` 不確定性量級**：

```cpp
struct Nondeterministic {
  unsigned uncertainty = 0;   // 0=完全確定(預設,zero-init)；數值愈高=不確定性愈高
                              // 對含 LLM 的程式：隨測試環節通過，數值往下降（馴化＝把它推向 0）
  std::optional<std::map<std::string, std::string>> extra;  // 證書細節：model/test_set/stability + 自訂
};
```

| 決策 | 選擇 | 說明 |
|---|---|---|
| **表示法** | **`unsigned uncertainty`（連續量級）** | 0＝完全確定；愈高愈不確定。非三態 bool\|object，非離散碼表。 |
| **預設** | **`0`（zero-init）** | 0＝完全確定＝最保守／預設，天然 zero-init。與軸 6 同漂亮性質。 |
| **證書去向** | **落 `extra`** | model/test_set/stability 進 extra（不結構化成 opt 欄位）。 |
| **單位** | **先留開** | 契約只釘「愈高愈不確定、馴化使其下降」；確切量綱（如 `100−stability%` 或抽象分數）暫不釘，待 v0 收緊。 |

### 為何量值優於三態（這軸的核心洞見）
馴化框架的工作正是「**把這個數字往 0 推**」——測試環節一個個通過，不確定性下降。
而 **0（完全確定）正是 roadmap 終局**：LLM 退縮、被確定性程式碼取代。
這條量尺把「是否隨機 + 認證到多穩」**統一成一條單調軸**，且讓馴化的 telos 在型別層就是字面的「→ 0」。
這個數字本質是一隻**債務儀表**（uncertainty debt gauge）。

### 三態如何摺進量尺
- 確定性（權威 false）＝ `uncertainty == 0`。
- 未認證隨機（開機期）＝ `uncertainty` 高，且 `extra` 無證書。
- 認證隨機（成熟期）＝ `uncertainty` 低，且 `extra` 帶 `{model, test_set, stability}`。
- 「認證 vs 未認證」**不再是硬邊界**，而是「數字多低 + extra 有無證書」。型別層不強制（同權威「從粗糙到嚴整」）。

### 與權威的分歧（記明）
1. 權威 `bool | object` 三態 → 本線收斂為**單一 `unsigned` 不確定性量級**（0＝確定）。
2. 證書的結構化 key（model/test_set/stability）→ 本線**降為 `extra`**（量值承載「多不確定」，extra 承載「憑什麼認證」）。
3. 「stability%」不再是獨立欄位，而是**以「數值離 0 多近」隱含表達**（馴化使其降）。

### 三層對位
固定＝無（整體可缺席，預設＝視同 uncertainty=0）；opt＝無；額外＝`extra`。
> 與軸 6 對照：軸 6＝開放碼表（離散具名 unsafe/safe/…）；軸 9＝不確定性量級（連續，0=確定）。
> 同為 `unsigned` + zero-init 保守預設，但一個是「分類碼」、一個是「量尺」。

---

## def/impl 定位（2026-06-24）

- **描述面（→ defs）✅**：`unsigned uncertainty` + `extra`——治理的資料載體，hub 必讀（不像軸 8 可推給 impl）。
- **設施面（→ impl）＝全九軸最大的一塊**：
  - **馴化框架**（retry / vote / guard …，見 `try_implement/docs/`）：把高 uncertainty 收斂到低，是 roadmap 核心願景的機器。
  - **證書簽發 / 撤照**：跑 `test_set × model` 算穩定度 → 更新 uncertainty（往下推）/ 撤照（往上彈）。
  - **與元件 1 LLM Entry Manager 串接**：隨機環節的實際 LLM 呼叫由 entry manager 路由。
  - **僅定位，待 impl 集中重做設計**（且應與整個馴化框架願景一起談）。

---

## 開放問題 / 後續輪次
- **單位／量綱**：uncertainty 的具體尺度（`100−stability%`？無界抽象分數？對數尺？）——待 v0 真實馴化資料逼出。
- **認證邊界**：是否要在某數值門檻或 extra 鍵齊全時，型別／工具層判定「已認證」——傾向不強制，待 v0。
- 設施面（馴化框架 + 簽發/撤照 + entry manager 串接）API——待 impl 集中重做，與 roadmap 願景對齊。
