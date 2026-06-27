# 軸 6：`interruptible`（可中斷性）

> 狀態：✅ 定案（Round 3，序→名目，supersede R2）。`unsigned level = 0`（**名目分類碼，禁大小比較**，0=unsafe 預設）+ 統一 `extra`。
> R3 撤掉 R2 的「有序階梯」框架（safe(1)/graceful(5) 在「能否直接 kill」上幾乎相反，本就非序）。type 不變。

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`_validate_interruptible`：str 須屬標準值集，或 dict 須含 `type`）、
`core_nature/lib_spec.md §5`、`core_nature/axis_spec.md §5`。

描述程式對**中斷（強制退出、kill、環境切換）的承受能力**及中斷後對外部狀態的影響。跨環境抽象，不綁 OS 信號。

**預設值**：缺席／null／false → `"unsafe"`（**最保守**）。⚠️ 注意：此軸保守端是「壞」的那端，與其他軸「預設＝簡單/安全的 false」相反。

### 標準字串值（6 個，是一條混合「損毀」與「恢復能力」的階梯）

| 值 | 語意 |
|---|---|
| `safe` | 可隨時中斷，無副作用，外部狀態不受影響 |
| `unsafe` | 中斷可能損毀外部狀態，或任務未完成致狀態未達預期（**預設**） |
| `resettable` | 中斷損毀，但提供重置機制可恢復到**某安全狀態**（不保證原始） |
| `rollback` | 中斷損毀，但提供回滾可**完整還原至呼叫前** |
| `resumable` | 可中斷且**從斷點續跑**；不損毀，下次自中斷處接續 |
| `graceful` | 可中斷但**不能直接 kill**；需給時間 cleanup 後正常退出 |

### 物件形式（補細節／非標準）
`{type, ...}`，`type` 必填。e.g. `{type: resettable, reset_hint: "--reset"}`、`{type: conditional, condition: "llm_entry 正常時可中斷"}`。
**`conditional` 及其他非標準型別屬自定義，value 不限** → 型別集**開放**。

### 跨軸關係
- §5a OS 排程層（單次執行內：不可中斷/可暫停 SIGSTOP/可恢復 checkpoint）；§5b 跨呼叫狀態層（無殘留/損毀/一致）。
- 與軸 3：`safe` 常配 `stateless`，但獨立。與軸 7 `guarantee`：§6 描述「中斷後如何恢復」，兩軸搭配語意更完整。

---

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

---

## 開放問題 / 後續輪次

- 設施面（signal/checkpoint/rollback helper）API ——待 impl 集中重做設計。
- §5a（單次執行內可暫停/恢復）目前未在 metadata 顯式建模；權威也只用單一 level 概括，暫不另開。
- 碼 ≥6 自定義若日後常用（如 conditional），可考慮升為標準碼。

---

## Round 3（✅ 序→名目，supersede R2，defs 重新思考 2026-06-27）

對照 `defs_review/axis_6_interruptible.md`，套「別管 hub」原則。定性：安全攸關軸，任何 caller
（人/script/hub）都要知道「能不能 kill」→ 非純 hub，「別管 hub」不溶掉它。

### 唯一實質改動（review ②）：`level` 從「有序階梯」改判「名目分類碼」

R2 把 0–5 當「越大越可恢復/越溫和」的有序階梯，但 ≥6 的 conditional 不在此單調軸上 →
unsigned「同時聲稱有序、又裝不可比值」自相矛盾。**更根本**：0–5 本就不是乾淨的序——
`safe(1)`＝隨時可 kill（最 kill-friendly）、`graceful(5)`＝不能直接 kill（最不 kill-friendly），
1 與 5 在「能否直接砍」幾乎相反；6 值混了損毀/恢復/能否 kill 三件事（Round 1 已自承）。

→ **正式撤掉序的框架：`level` 是名目分類碼（nominal，像 errno），禁大小比較。**
矛盾消失（無序的宣稱 → ≥6 不再破壞序）。conditional 細節照舊進 extra。

**type 維持 `unsigned` 開放碼表**——更貼軸 7 判準：軸 7 因「值集封閉」用 enum；軸 6 權威明確允許
conditional/自定義（值集開放）→ 同一判準在此導出「開放碼表」。errno 即此形狀：開放、具名、無序。

### review 另兩條：不改

- **① zero-init 抹平信任級別**：對安全軸，「沒想過」當「unsafe」對待本就正確且最保守——
  正是 zero-init=unsafe 的漂亮處（忘填→自動落最保守端）。review 要的「未宣告 vs unsafe」更細區分
  （hub 先問 vs 別 kill）是 hub kill 策略 → 別管 hub，丟掉。**保留 zero-init，不上 optional。**
- **③ graceful × resumable 正交**：接受限制，不拆正交三欄（entries 過度拆解的教訓）。
  真同時具備者填操作上更關鍵那個、另一個註記 extra。

### 型別草圖（定案）

```cpp
struct Interruptible {
  static constexpr unsigned unsafe=0, safe=1, resettable=2,
                            rollback=3, resumable=4, graceful=5;
  unsigned level = 0;   // 名目分類碼（nominal，像 errno）；≥6 自定義；禁大小比較；0=unsafe(zero-init)
  std::optional<std::map<std::string,std::string>> extra;   // condition/reset_hint/正交補充
};
```
