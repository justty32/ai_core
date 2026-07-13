# 軸 6 interruptible — 封存：被 supersede 的舊輪次（Round 1–2）

> 封存（2026-06-28 收斂）。含「有序階梯」的舊框架（已改判名目碼），保留 WHY，**非現況**。現況見 [`../axis_6_interruptible.md`](../axis_6_interruptible.md) 與事實基準 `defs/axes.hpp`。

## Round 1（提案）

### def/impl 定位
- **描述面（→ defs）**：中斷承受能力的分類，給 hub 讀（本軸濃縮對象）。
- **設施面（→ impl）**：signal 處理（SIGTERM 後 graceful cleanup）、checkpoint/resume、reset/rollback 機制。
  resumable/rollback/resettable 的真實機器可由 lib 提供。**僅定位，待 impl 集中重做設計。**

### 關鍵觀察：套用「entries 最終定案」的教訓
entries 曾大幅拆成正交 enum（Round 2–6），最後**收回成「扁平 + extra」**（Round 10/11）——教訓：別過度拆解。
interruptible 的 6 值雖混了「損毀/恢復/kill」三件事，**可拆成正交屬性**（`damages` / `recovery` / `graceful`），
但那正是 entries 走過頭再收回的路。且其型別集**開放**（conditional/custom）→ 與 entries content（Round 7）同境：
**開放集合 → 自由字串**。

### 提案：扁平單分類 + extra（與 entries 最終形同構）

```cpp
struct Interruptible {
  std::string level = "unsafe";   // 固定欄位 ← 最小必須；標準值 safe/unsafe/resettable/rollback/resumable/graceful；開放
  std::optional<std::map<std::string, std::string>> extra;  // 額外：reset_hint/condition/自定義 type 細節
};
```

三層對位：**固定**＝`level`（恆在、保守預設 `"unsafe"`）；**opt**＝無；**額外**＝`extra`（reset_hint/condition…）。

> **設計張力**（同 entries content Round 7）：自由字串什麼都能塞，違反「讓非法狀態無法被表達」。
> 但成立——型別集本質開放（conditional/custom），強行 enum 徒勞。標準 6 值當「建議值」，非強制。

### 判斷點（待你拍）
- **D1 表示法**：(a) 扁平 `string level` + extra〔建議，同 entries 終局〕；(b) 閉合 `enum`（6 值，但 conditional/custom 無處放）；(c) 正交拆解 `damages`/`recovery`/`graceful`（最「嚴謹」但重蹈 entries 過度拆解）。
- **D2 預設**：`"unsafe"`（保守端）——確認接受「此軸預設＝壞那端」這個與他軸相反的特例？
- **D3 標準值**：6 個標準字串當**建議值**（不強制、可填 conditional/custom）— 採？

---

## Round 2（✅ 拍板）

D1 表示法從「扁平 string / enum / 正交拆解」三案外，收斂到**開放碼表 `unsigned`**：

| 決策 | 選擇 | 說明 |
|---|---|---|
| **D1 表示法** | **`unsigned level`（開放碼表）** | 非 enum、非 string。像 errno/exit code/HTTP status：標準先釘碼，後面留擴充。 |
| **D2 預設** | **`0`（＝unsafe，靠 zero-init）** | 不必特別指定預設值，**zero-init 天然即最保守 unsafe**。漂亮性質。 |
| **D3 標準值** | **碼表 (i)：標準釘 0–5，≥6 自定義** | 保留權威完整 6 值階梯、跨工具碼一致；6 以上自由擴充。 |

**為何 unsigned 而非 int**：等級碼非負，unsigned 天然排除負值 → 多一分「讓非法狀態無法被表達」。

**為何碼表而非 enum/string**：
- enum 是封閉集合，裝不下權威允許的 `conditional`/自定義 type。
- 碼表 = 開放且有序，zero-init=unsafe 又恰好給出保守預設；具名常數補回可讀性。
- 設計張力（記明）：unsigned 仍可塞任意值（最不「非法狀態無法表達」），換來零成本擴充——與「隨意添加」需求一致。同 entries content（Round 7）/ resources（自由 key）走過的「開放集合」取捨。

### 標準碼表

| 碼 | 名稱 | 語意 |
|---|---|---|
| 0 | `unsafe` | 中斷可能損毀外部狀態或任務未完成（**預設，zero-init**） |
| 1 | `safe` | 可隨時中斷，無副作用 |
| 2 | `resettable` | 損毀但可重置到某安全狀態 |
| 3 | `rollback` | 損毀但可完整還原至呼叫前 |
| 4 | `resumable` | 不損毀，可從斷點續跑 |
| 5 | `graceful` | 不能直接 kill，需 cleanup 後正常退出 |
| ≥6 | 自定義 | 各自約定（如 conditional）；type 名/條件放 `extra` |

### 型別草圖（定案）

```cpp
struct Interruptible {
  // 標準碼表常數（具名、補回可讀性）
  static constexpr unsigned unsafe = 0, safe = 1, resettable = 2,
                            rollback = 3, resumable = 4, graceful = 5;
  unsigned level = 0;                                       // 固定欄位；0=unsafe(zero-init 預設)
  std::optional<std::map<std::string, std::string>> extra; // 額外：reset_hint/condition/自定義 type 細節
};
```

三層對位：固定＝`level`；opt＝無；額外＝`extra`。
