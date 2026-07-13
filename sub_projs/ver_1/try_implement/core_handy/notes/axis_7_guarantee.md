# 軸 7：`guarantee`（執行保證）

> ⚠️ 歷史討論記錄（2026-06-28 收斂）。**事實基準在 `../defs/axes.hpp`**（本軸已驗證對齊）。本軸 Round 1 即定案，無被取代輪次；本檔保留設計脈絡。

> **2026-06-28 再收斂：軸 7 內聯進 `../defs/axes.hpp` 的 `Meta`（裸 `Guarantee guarantee`，移除 `GuaranteeField` 包裝）；extra 上收為單一 `Meta::extra`。**

> 狀態：✅ 定案（Round 1）。封閉 `enum class : unsigned`（none=0 預設）；enum Guarantee 內聯進 Meta（裸欄位），extra 收斂至 `Meta::extra`。

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`_GUARANTEE_VALUES = {none, idempotent, transactional}`，
`v not in _GUARANTEE_VALUES` 即報錯）、`docs/spec/lib_spec.md §6`、`docs/spec/axis_spec.md §6`。

描述程式對其執行**副作用的承諾**，尤其失敗或中斷後的**狀態一致性**。

### 標準值（封閉三值，有序階梯：弱 → 強）

| 值 | 語意 |
|---|---|
| `none` | 無承諾（**預設**）；重複執行或中途失敗後果由呼叫方自負 |
| `idempotent` | 重複執行 ≡ 執行一次；中斷後安全重試，不累積額外副作用 |
| `transactional` | 全成功或全不發生（ACID）；中途失敗自動回滾，不留部分狀態 |

- **封閉集合**：`_core.py` 僅驗證屬此三值。**無物件形式、無 custom 逃生艙**（與軸 6 截然不同）。
- **預設 `none`**：缺席／null → `none`。保守端＝「弱承諾」那端，恰好是 zero 位。
- **有序蘊含**：transactional 實質也蘊含可安全重試（強承諾涵蓋弱承諾）。

### 跨軸關係
- **與軸 3 `state`**：對 `stateless` 通常**無意義**（stateless 本身即等同冪等、無外部狀態可回滾）。
  有意義的宣告限 `stateful_external`。
- **與軸 6 `interruptible`**：§5 講「中斷承受力」、§6 講「中斷後如何恢復」。
  `rollback`(軸6) × `transactional`(軸7) 同向；`unsafe` × `idempotent` ＝可安全重試。
- **與軸 8 `dry_run`**：正交，可並存（transactional + dry_run、none + dry_run…）。

---

## Round 1（✅ 拍板）

compact 前曾隨口推測「開放碼表大概也適合軸 7」——**讀權威後收回**。軸 7 與軸 6 表面同形
（有序階梯＋保守預設），本質相反，故慣用法相反：

| | 軸 6 interruptible | 軸 7 guarantee |
|---|---|---|
| 權威值集 | **開放**（明文允許 conditional/自定義 type） | **封閉**（就 3 值，無 custom、無物件形式） |
| 物件形式 | 有（`{type, reset_hint…}`） | **無** |
| 慣用法 | 開放碼表 `unsigned`（裝得下 custom） | **封閉 `enum class`**（讓非法狀態無法被表達） |

> C++ 線的額外原則「讓非法狀態無法被表達」在軸 6 被迫犧牲（集合本質開放）；
> **軸 7 正好能還它**。在能還的地方還，比「為與軸 6 一致而硬開放」更嚴謹——
> 分歧由權威本身的封閉 vs 開放正當化。

### 判斷點（已拍）

| 決策 | 選擇 | 說明 |
|---|---|---|
| **D1 表示法** | **封閉 `enum class : unsigned`** | 封閉集合的教科書解；非法值無法被表達。3 值有序。 |
| **D2 extra** | **開**（保留統一逃生艙） | 雖權威無物件形式，仍掛 `optional<map<string,string>>` 統一逃生艙（與全軸一致；日後若出現如回滾機制 hint 可承載）。 |
| **D3 預設** | **`none`（zero-init）** | 沿用「保守端＝弱承諾」；none=0 → zero-init 天然最保守。 |

### 型別草圖（定案）

```cpp
enum class Guarantee : unsigned {
  none          = 0,   // 無承諾（預設，zero-init）；重試/中斷後果呼叫方自負
  idempotent    = 1,   // 重複執行 ≡ 執行一次；中斷後安全重試
  transactional = 2,   // 全成功或全不發生（ACID）；中途失敗自動回滾
};

// 2026-06-28 再收斂：移除 GuaranteeField 包裝，guarantee 直接內聯成 Meta 的裸欄位：
//   Guarantee guarantee = Guarantee::none;   // zero-init=none
// 額外逃生艙（回滾機制 hint / 冪等鍵帶法）PARKED 至單一 Meta::extra（全軸共用）。
```

三層對位：固定＝Meta 裸欄位 `guarantee`；opt＝無；額外＝統一 `Meta::extra`。

> 與軸 6 的形狀對照：軸 6 用 `unsigned level`（開放碼表，因值集開放）；軸 7 用 `enum class`
> （封閉，因值集封閉）。兩軸都「有序＋extra」，但表示法分歧——這是**權威值集開閉不同**的直接結果。
> 是否獨立成 struct 還是內聯進總 metadata，留全軸收尾統一決定。

---

## def/impl 定位（2026-06-24）

- **描述面（→ defs）**：`enum class Guarantee` 值——只回答「對外部狀態做了什麼承諾」，給 hub/呼叫方讀。
- **設施面（→ impl）**：冪等/交易機器（journal、commit/rollback、dedup key）。
  **與軸 6 的 `rollback`/`resettable` 設施高度重疊**——「中斷能回滾」（軸6能力）與「執行具事務性」
  （軸7承諾）共用同一套 transaction/journal helper。歸同一設施，**僅定位，待 impl 集中重做設計**。

---

## 開放問題 / 後續輪次

- 設施面（transaction/journal helper）API——與軸 6 rollback 設施合併設計，待 impl 集中重做。
- `idempotent`/`transactional` 對 `stateless` 無意義，是否型別層跨軸強制（軸 3）——傾向不強制，文件約束即可（同軸 5 idle×persistent）。
- 碼 ≥3 的擴充（如 eventually-consistent / at-least-once）：權威已凍結 3 值，若日後要加屬規範層決策，再回頭評估是否改開放碼表。
