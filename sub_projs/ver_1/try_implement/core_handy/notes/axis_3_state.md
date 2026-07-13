# 軸 3：`state`（跨呼叫狀態）

> ⚠️ 歷史討論記錄（2026-06-28 收斂）。**事實基準在 `../defs/axes.hpp`**（本軸已驗證對齊）。本檔只留最終拍板與現況脈絡；被取代的 Round 1 見 [`archived/axis_3_history.md`](archived/axis_3_history.md)。

> 狀態：✅ 定案（Round 2）。單一 `bool stateful = false`（false=stateless 預設、true=stateful_external），無 detail。

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`_STATE_VALUES = {stateless, stateful_external}`）、
`docs/spec/lib_spec.md §3`、`docs/spec/axis_spec.md §3`。

`state` ＝ **單次執行以外，是否保有／影響／累積跨呼叫的狀態**。既有設計**只有兩個值**：

| 值 | 語意 |
|---|---|
| `stateless` | 每次執行互相獨立，輸出完全由當次輸入決定。可安全重試、可平行。 |
| `stateful_external` | 狀態存於程式**外部**（檔案、DB、環境變數）。呼叫方需知悉此程式會對外部狀態讀寫，產生副作用。 |

- **`stateful_internal` 被刻意排除**：狀態存程序記憶體，process 結束即釋放；要持久化只能寫進外部檔案
  → 實務上等同 stateful_external。既有設計明言「此分類實務上可視為不存在」。
- **預設值（既有設計）**：缺席／null／false → `stateless`。只有明確需要時才宣告 `stateful_external`。
- **關注點是「有沒有跨呼叫副作用」，不是「怎麼傳輸／存哪」**：
  - 狀態**存在哪**（路徑慣例 config/cache/state/data）→ 軸 4 `state_dirs` 的事。
  - 狀態**怎麼進出** → 軸 1 `entries`（與 §1 通訊出入口有重疊，但本軸不管傳輸）。

### 與軸 2 `lifecycle` 的正交組合（既有設計）

| 組合 | 常見形態 |
|---|---|
| one_shot + stateless | 最典型 CLI：輸入→輸出→結束（兩者皆預設，可全省） |
| one_shot + stateful_external | DB 更新腳本、append log：每次執行都改外部狀態 |
| persistent + stateless | 無記憶的 stateless server（每請求獨立） |
| persistent + stateful_external | 有狀態 server（session、累積計數器，寫外部） |

→ state 與 lifecycle 正交、不可互相推導，兩軸都該留。

---

## Round 2（✅ 拍板）

| 決策 | 選擇 | 說明 |
|---|---|---|
| **D1 型別 + 命名** | **`bool stateful`** | 兩值無廢狀態，bool 即足；`internal` 已折除，`stateful` 唯一等價 `external`，不帶後綴。 |
| **D2 預設** | **`false`（stateless）** | 比照既有強預設，絕大多數工具無狀態。 |
| **D3 detail** | **不開** | state 無同級變體要承載（存哪→軸4、怎麼傳→軸1、internal 已折除）。純 bool flag。 |

### 型別草圖（定案）

```cpp
bool stateful = false;   // false=stateless(預設)、true=stateful_external
```

> 是否獨立成 struct 還是內聯進總 metadata，留到全軸收尾統一決定（與軸 2 一致性）。

---

## def/impl 定位（2026-06-24 複查）

**軸 3 是 def，其 impl 是軸 4。**

- 描述面（→ defs）：`bool stateful`——只回答「**有沒有**外部狀態」。
- 設施面（→ impl）：**無獨立設施**；「**怎麼管**外部狀態」完全委派給**軸 4 狀態託管**。

→ 軸 3／軸 4 是一對互補：軸 3 說「有狀態」（描述），軸 4 提供「託管它」（設施）。
對稱關係：軸 4 純 impl、描述面外包給軸 3；軸 3 純 defs、設施面外包給軸 4。
