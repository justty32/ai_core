# 軸 4 state_dirs — 封存：已過時的 Round 1 描述性欄位提案

> 封存（2026-06-28 收斂）。此提案已被「軸 4＝純設施」的重定位取代，**非現況**。現況見 [`../axis_4_state_dirs.md`](../axis_4_state_dirs.md) 的 Round 2 與事實基準 `impl/state.hpp`。

## Round 1（提案）〔已過時——舊的描述性欄位提案，保留作脈絡〕

> 註：以下把 state_dirs 當「描述性 metadata 欄位」來濃縮。經 2026-06-24 重定位，軸 4 改為「設施 API」，
> 本節結論不再代表現況；新方向見上方頭部與下方「設施設計」。

這是**第一個「值域是一組已知值的子集」**的軸（前三軸都是純二元）。濃縮的核心在 D1 表示法。

### D1 表示法：4 個 bool（建議）
Python 用 `list[str]` + runtime 檢查子集，**允許非法字串、允許重複**。C++ 要「讓非法狀態無法被表達」，
兩條路：

| 方案 | 樣子 | 優劣 |
|---|---|---|
| **4 bool（建議）** | `struct { bool config, cache, state, data; }` 全預設 false | 無非法值、無重複；延續前三軸的 flat-bool 風格，一致性最高；缺點是不像「集合」 |
| enum set | `std::set<StateDir>` | 像集合、無重複；但引入 enum，且 set 比 4 bool 重 |
| bitset/flags | `std::bitset<4>` 或 enum flags `|` | 緊湊；但位元語意對讀者不如具名 bool 直觀 |

→ 建議 **4 bool**：與 entries/lifecycle/state 的取向一致，非法狀態自然消滅。

### D2 預設：四個皆 false（建議）
- 全 false ＝「未用任何標準目錄／未宣告」。這把 Python 的「缺席」與「明確宣告空集」合流為同一狀態。
- 可接受：本軸僅資訊性，`stateful` 已獨立通知副作用存在；用非標準後端（如純 DB）的 stateful 程式，全 false 也正確。

### D3 與軸 3 `state` 的關係（**要你定奪的重點**）
存在一個潛在非法狀態：**stateless 卻宣告了 state_dirs**（無狀態哪來的狀態目錄？）。兩種處理：

- **(a) 維持兩軸獨立**（貼權威）：`bool stateful` 與 `StateDirs` 各自存在，靠慣例/文件約束「stateless 時 dirs 應全 false」。簡單、與既有規範一一對應，但非法組合在型別上仍可表達。
- **(b) 把 dirs 巢進 stateful**：`std::optional<StateDirs> stateful;`——**有值＝stateful_external 且這些是用到的目錄；無值＝stateless**。一舉讓「stateless+dirs」無法表達，把軸 3、軸 4 合成一個欄位。代價：軸 3 已拍板為獨立 `bool`，這等於回頭重開；且「stateful 但未宣告 dirs」要用「空的 StateDirs（全 false）」表示，與 (a) 的全 false 同義。

→ 我的傾向：**(b)**，因為它正是這條 C++ 線的賣點（讓非法狀態無法被表達），且軸 3、軸 4 在權威設計裡本就是同一個複合慣例的兩面。但這會**回頭調整軸 3 的定案**，所以交給你拍。

### 型別草圖

```cpp
// 方案 (a)：兩軸獨立
struct StateDirs { bool config=false, cache=false, state=false, data=false; };
// 配 軸3 的 bool stateful;

// 方案 (b)：合併——presence=stateful，內容=用到的標準目錄
struct StateDirs { bool config=false, cache=false, state=false, data=false; };
std::optional<StateDirs> stateful;   // 取代 軸3 的 bool stateful
```

---
