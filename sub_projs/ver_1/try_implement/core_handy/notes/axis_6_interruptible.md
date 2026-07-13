# 軸 6：`interruptible`（可中斷性）

> ⚠️ 歷史討論記錄（2026-06-28 收斂）。**事實基準在 `../defs/axes.hpp`**（本軸已驗證對齊）。本檔只留最終拍板（Round 3）與現況脈絡；被取代的 Round 1–2 見 [`archived/axis_6_history.md`](archived/axis_6_history.md)。

> 狀態：✅ 定案（Round 3，序→名目，supersede R2）。`unsigned level = 0`（**名目分類碼，禁大小比較**，0=unsafe 預設）+ 統一 `extra`。
> R3 撤掉 R2 的「有序階梯」框架（safe(1)/graceful(5) 在「能否直接 kill」上幾乎相反，本就非序）。type 不變。
>
> **2026-06-28 再收斂：extra 已上收為單一 `../defs/axes.hpp` 的 `Meta::extra`；本軸 `Interruptible` 移除 extra 欄位（保留 struct 與具名常數，condition/reset_hint 改走 Meta::extra）。**

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`_validate_interruptible`：str 須屬標準值集，或 dict 須含 `type`）、
`docs/spec/lib_spec.md §5`、`docs/spec/axis_spec.md §5`。

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
  // condition/reset_hint/正交補充改走 Meta::extra（extra 已上收為單一 ../defs/axes.hpp 的 Meta::extra）
};
```

---

## 開放問題 / 後續輪次

- 設施面（signal/checkpoint/rollback helper）API ——待 impl 集中重做設計。
- §5a（單次執行內可暫停/恢復）目前未在 metadata 顯式建模；權威也只用單一 level 概括，暫不另開。
- 碼 ≥6 自定義若日後常用（如 conditional），可考慮升為標準碼。
