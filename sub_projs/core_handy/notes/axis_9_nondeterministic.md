# 軸 9：`nondeterministic`（確定性 / 隨機性・治理證書）

> ⚠️ 歷史討論記錄（2026-06-28 收斂）。**事實基準在 `../defs/axes.hpp`**（本軸已驗證對齊）。本軸 Round 1 即定案，無被取代輪次；本檔保留設計脈絡。
>
> **2026-06-28 再收斂：軸 9 內聯進 `../defs/axes.hpp` 的 `Meta`（裸 `unsigned uncertainty`，不再包 struct）；治理證書改走單一 `Meta::extra`。**

> 狀態：✅ 描述面定案（Round 1）。單一 `unsigned uncertainty = 0`（0＝完全確定；愈高愈不確定；馴化使其下降）+ `extra`。
> 九軸的靈魂軸：治理原則「拒絕為預設＋憑證准入」的資料載體。**九軸描述面到此收尾。**
>
> **設施面開工（2026-06-28）**：A 馴化原語落地 `../impl/taming.hpp`（`vote` 多數決 / `best_of` 打分；L3 聚合「降方差」段，
> 共用 `sample()` 蒙地卡羅核心，組合子可疊）；驗證見 `../examples/taming_demo.cpp`。
> B 證書引擎**只定形狀**（見 §3，status/model/test_set/stability → `Meta::extra`）；簽發/撤照引擎待 v0 真 test_set 逼出。
> retry/guard/memoize（L1/L2）亦待真消費者逼出。

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

> **2026-06-28 再收斂**：以下不再包 `struct Nondeterministic`。軸 9 **內聯為 `Meta` 的裸欄位**，
> 證書細節改走 `Meta` 唯一的 `extra` 袋（全軸共用）。

```cpp
struct Meta {
  // …前八軸欄位…
  unsigned uncertainty = 0;   // 軸 9（債務儀表）：0=完全確定(預設,zero-init)；數值愈高=不確定性愈高
                              // 對含 LLM 的程式：隨測試環節通過，數值往下降（馴化＝把它推向 0）
  Extra    extra;            // ★ 全軸共用的單一逃生艙；軸 9 證書細節（model/test_set/stability + 自訂）落於此
};
// 序列化：軸 9 扁平化為 "uncertainty":N（不再是 "nondeterministic":{"uncertainty":N}）
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

## 3. 設施面落地與證書形狀（2026-06-28）

設施面分兩塊（範圍決策：**A 全做、B 只定形狀**）：

### A. 馴化原語（✅ 落地 `../impl/taming.hpp`）

把「不可靠的 `f(str)→str`」收成「可靠的」。本輪只取 **L3 聚合**兩支（蒙地卡羅 + 打分）——
其餘層（L0 契約 bind＝字串拼接、L1 確定化 memoize、L2 驗證 retry/guard、L4 編排 hub/router）
待真消費者逼出。原語是**組合子（decorator）**：吃 `Fn` 回 `Fn`，可層層疊。

| 原語 | 層 | 語意 | 平手規則 / 邊界 |
|---|---|---|---|
| `sample(f, in, n)` | 核心 | 同輸入抽 n 次、回 `vector<string>` | n≤0 → 空 |
| `vote(f, n)` | L3 | self-consistency 多數決（蒙地卡羅）；同答案抽多了浮上來 | 平手取先抽到者（`>` 非 `>=`）；n=1 退化原函式；n≤0 回空 |
| `best_of(f, n, score)` | L3 | 抽 n 次、`score`（高=好）取最高分；冷熱分工 f 熱·score 冷 | 平手取先抽到者；n≤0 回空 |

> 與描述面對位：本層是「把 `uncertainty` 往 0 推」的**降方差段力**。但**只跑、不寫 uncertainty**——
> 改寫 `Meta::uncertainty` / 簽證書是 B 證書引擎的職責（本輪未做）。

### B. 證書形狀（只定形狀，引擎延後）

roadmap §3.4：證書＝「此環節經測試組 Y、用模型 X、認證穩定度 Z%」，寄生 `Meta::extra`（string→string）。
**本輪只釘形狀，不蓋引擎**（無真 test_set/asset 可認證 → 憑空抽象，違反「無消費者就延後」）。

證書在 `Meta::extra` 的約定鍵（沿用 §1 權威三鍵 + roadmap §3.5 的 status）：

| `extra` 鍵 | 值（string） | 語意 |
|---|---|---|
| `cert.status` | `uncertified` / `syntax_ok` / `rejected` | 治理狀態；`rejected` 另帶 reason |
| `cert.reason` | `guardrail_violation` / `retry_exhausted` | 僅 rejected：安全否決 vs 能力不足（撤照意義不同） |
| `cert.model` | 如 `qwen2.5` | 用哪個模型認證 |
| `cert.test_set` | 如 `code_edit_v0` | 經哪組測試 |
| `cert.stability` | 如 `92`（百分比整數） | 認證穩定度% |

> 為何寄生 `extra` 而非結構化欄位：沿用軸 9 描述面決策——量值（`uncertainty`）承載「多不確定」、
> `extra` 承載「憑什麼認證」；「從粗糙到嚴整」，待 v0 真資料收緊。鍵用 `cert.` 前綴避免與其他軸的 extra 撞名。

### 證書引擎（⏸ 延後，待 v0）

簽發＝跑 `test_set × model` → 算 pass rate（穩定度）→ 寫上表 `cert.*` + 降 `uncertainty`；
撤照＝重跑掉到門檻下 → 清 `cert.*` + 彈回 `uncertainty`。消費者＝roadmap §6 v0 切片
「框架→生資產→笨模型+馴化做出正確程式碼編輯」那條線——它跑出來才有 asset/test_set 可認證。
原語（A）已備好當引擎零件（retry/guard 補上後，引擎用它們跑 test_set）。

---

## def/impl 定位（2026-06-24，2026-06-28 更新落地狀態）

- **描述面（→ defs）✅**：內聯 `unsigned uncertainty`（裸欄位，不再包 struct）；治理證書走 `Meta::extra`——治理的資料載體，hub 必讀（不像軸 8 可推給 impl）。
- **設施面（→ impl）＝全九軸最大的一塊**（2026-06-28 部分落地，見 §3）：
  - **馴化原語**（見 `../impl/taming.hpp`）：✅ L3 聚合 `vote`/`best_of` 落地（降方差）；retry/guard/memoize 待真消費者逼出。
  - **證書簽發 / 撤照**：⏸ 只定形狀（§3 B，`cert.*` 走 `extra`）；引擎（跑 `test_set × model` → 更新 uncertainty / 撤照）待 v0 切片逼出。
  - **與元件 1 LLM Entry Manager 串接**：隨機環節的實際 LLM 呼叫由 `../examples/llm_entry.cpp` 路由；馴化原語包在其外。
  - 餘下與整個馴化框架願景一起談（v0）。

---

## 開放問題 / 後續輪次
- **單位／量綱**：uncertainty 的具體尺度（`100−stability%`？無界抽象分數？對數尺？）——待 v0 真實馴化資料逼出。
- **認證邊界**：是否要在某數值門檻或 extra 鍵齊全時，型別／工具層判定「已認證」——傾向不強制，待 v0。
- 設施面（馴化框架 + 簽發/撤照 + entry manager 串接）API——待 impl 集中重做，與 roadmap 願景對齊。
