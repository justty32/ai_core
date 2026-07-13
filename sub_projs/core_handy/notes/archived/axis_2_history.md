# 軸 2 lifecycle — 封存：被取代的早期輪次（Round 1–2）

> 封存（2026-06-28 收斂）。保留 WHY，**非現況**。現況見 [`../axis_2_lifecycle.md`](../axis_2_lifecycle.md) 與事實基準 `defs/axes.hpp`。

---

## Round 1（提案）

濃縮空間不在砍欄位（已 2 值極簡），在三個判斷：D1 型別（enum vs bool）、D2 預設值、D3 變體去向。
（完整提案見對話；結論見 Round 2。）

跨軸連結：`streaming` 正是 entries 砍掉的 `mode`（batch/streaming）的另一面——兩個尾巴可一起安置。

---

## Round 2（✅ 拍板）

| 決策 | 選擇 | 說明 |
|---|---|---|
| **D1 型別** | **bool** | `persistent`：false=one_shot、true=persistent。兩值皆合法無廢狀態，故 bool 即可（不需 enum 殺廢狀態）。 |
| **D2 預設值** | **預設 false（one_shot）** | 比照既有設計的強預設；**不**跟 entries 的「無預設」走（絕大多數工具是 one_shot）。 |
| **D3 變體去向** | **`std::optional<std::string>` 輔助描述** | lazy/warm、periodic、streaming、detached… 一律當自由文字描述承載，不結構化、不 enum，缺席＝無。 |

> singleton 仍明確歸 resources 軸，不在 lifecycle。

### 型別草圖

```cpp
struct Lifecycle {
  bool persistent = false;             // D1+D2：false=one_shot(預設)、true=persistent
  std::optional<std::string> detail;   // D3：輔助描述（lazy/warm、periodic、streaming、detached…）
};
```

---
