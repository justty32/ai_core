# 軸 3 state — 封存：被取代的早期輪次（Round 1）

> 封存（2026-06-28 收斂）。保留 WHY，**非現況**。現況見 [`../axis_3_state.md`](../axis_3_state.md) 與事實基準 `defs/axes.hpp`。

## Round 1（提案）

結構上與軸 2 `lifecycle` 幾乎同形：**兩個值、強預設到「簡單」那端**。三個判斷：

### D1 型別：bool（建議）
- 只有兩個合法值，無廢狀態 → 不需 enum 殺廢狀態，`bool` 即足。
- 命名：因 `stateful_internal` 已折除，`stateful` 就唯一等價於 `stateful_external`，不必帶 `_external` 後綴。
  - 候選：`bool stateful`（簡）vs `bool stateful_external`（貼權威字面）。建議 `stateful`。

### D2 預設：false = stateless（建議）
- 比照既有設計的強預設（絕大多數工具 stateless）。與 lifecycle 的強預設一致，**不**走 entries 的「無預設」。

### D3 變體去向 / 要不要 detail 欄位
- 這是與 lifecycle **唯一的差別**：lifecycle 留了 `optional<string> detail` 承載 lazy/warm/periodic… 等變體；
  但 state **沒有同級的變體要承載**——
  - 「存哪」歸軸 4 `state_dirs`、「怎麼傳」歸軸 1 `entries`、`stateful_internal` 已折除。
- **建議：state 不要 detail 欄位**，就是一個純 bool flag。比 lifecycle 更乾淨。
  - 若日後真有自由描述需求，再追加 `optional<string>`，不預先開。

### 型別草圖（建議案）

```cpp
bool stateful = false;   // false=stateless(預設)、true=stateful_external
```

> 注意：這軸濃縮後可能**連 struct 都不需要**，單一 bool 即可。是否獨立成型別、或內聯進總 metadata
> struct，留待全軸定案後一起決定（與 lifecycle 的 `Lifecycle` struct 取捨一致性問題）。

---
