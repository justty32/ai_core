# 軸 2：`lifecycle`（生命週期）

> ⚠️ 歷史討論記錄（2026-06-28 收斂）。**事實基準在 `../defs/axes.hpp`**（本軸已驗證對齊）。本檔只留最終拍板與現況脈絡；被取代的早期輪次見 [`archived/axis_2_history.md`](archived/axis_2_history.md)。

> 狀態：✅ 定案（Round 4 複查確認）。`bool persistent`（預設 false=one_shot）；extra 收斂至 `Meta::extra`。
> Round 3 的形狀不動；Round 4 只做兩件事：流動模式正式判給軸 1、軸 2 不收。
>
> **2026-06-28 再收斂：軸 2 內聯進 `../defs/axes.hpp` 的 `Meta`（裸 `bool persistent`，不再包 struct）；extra 上收為單一 `Meta::extra`。**

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`_LIFECYCLE_VALUES = {one_shot, persistent}`）、`core_nature/lib_spec.md §2`、
`core_nature/axis_spec.md §2`、`core_nature/execution_forms.md §4`。

`lifecycle` ＝ **process 從啟動到結束的持續模式**。既有設計**只有兩個值**：

| 值 | 語意 |
|---|---|
| `one_shot` | 啟動 → 執行 → 結束。process 壽命與任務等長，**做完自己結束** |
| `persistent` | 啟動後持續存活，待命或持續產出，**直到被顯式終止** |

- **本質分界**：自我終止（one_shot）vs 顯式終止（persistent）。二元、正交、不可由他軸推導 → 該留。
- **預設值（既有設計）**：缺席／null／false → `one_shot`。
- **其餘形態全是歸約，不另立 lifecycle 值**：
  - lazy/warm（首呼才啟動、idle 後關）→ persistent 的啟動策略（既有設計標「待定」）
  - detached/fire-and-forget → one_shot 的排程策略，是**呼叫方**的關切
  - periodic/daemon → persistent + 定時觸發
  - **streaming**（邊算邊輸出）→ **one_shot 的輸出變體**（process 角度仍 one_shot）；push vs pull(generator)
  - singleton → **不是 lifecycle，是資源約束**（歸 resources，可疊在 persistent 上）

---

## Round 3（✅ 修正：`detail` 併入統一 `extra`）

按「三層 ↔ 三欄位通則」（見 `00_index.md`），原 `optional<string> detail` 是裸字串，
不符通則的欄位種類。決定**併入全軸共用的 `extra`**（`optional<map<string,string>>`），lifecycle 不另開 string。
lazy/warm、periodic、streaming、detached… 一律當 extra 的 key-value 承載。

| 定義層 | 欄位 |
|---|---|
| 最小必須（固定） | `bool persistent = false` |
| 推薦（opt） | （無） |
| 額外（extra） | `optional<map<string,string>>`（承載 lazy/warm/periodic/streaming/detached…） |

### 型別草圖（定案）

> 2026-06-28 再收斂：不再有獨立 `struct Lifecycle`，軸 2 直接內聯成 `Meta` 的一個裸欄位；本軸不另帶 extra，變體描述改掛全軸共用的 `Meta::extra`。

```cpp
struct Meta {
  // ...
  bool persistent = false;   // 軸 2：false=one_shot(預設)、true=persistent
  // ...
  Extra extra;               // ★ 全軸共用單一 extra（承載 lazy/warm/periodic/streaming… 等變體描述）
};
```

> singleton 仍歸 resources 軸；streaming＝one_shot 輸出變體，現以 `Meta::extra` 承載（不另立流動軸，需要時再升級）。

---

## def/impl 定位（2026-06-24 複查）

> 設施面**僅定位、不設計 API**。之後會回頭重看全部 impl 筆記、統一重做實作設計。

### 描述面（→ defs）✅ 已定案
內聯 `bool persistent`（裸欄位，不包 struct）；extra 收斂至 `Meta::extra`（見 Round 3 + 2026-06-28 再收斂）。給 hub 讀：自我終止 vs 顯式終止。

### 設施面（→ impl）有，且與軸 1 同源
- `one_shot` 端**不需設施**：「跑完就 exit」是 process 模型本身，OS 已做掉（同軸 1 pipe 最原始格）。
- `persistent` 端**需設施＝server 化 helper**：把 one-shot 函數本體**託管成長駐 server**，
  讓多個 one-shot caller 連同一 process。對應 Python 側 `llm_entry_manager --socket`（底層 `lib/server.serve_socket`）。

| 設施涵蓋 | 說明 |
|---|---|
| serve loop | 保持存活、收請求、逐個派發給函數本體、顯式終止時收尾 |
| 傳輸接入 | unix socket / tcp / FIFO 收連線——**即軸 1 的 tcp/pipe 傳輸** |
| 單例＋佇列 | 一次一 process（singleton → 軸 5 resources）、請求排隊（LLM 單資源模式） |

### impl 分層骨架（跨軸，待之後集中重做設計）
```
軸 1 統一 I/O（檔案/pipe/shm/tcp） ← 地基
   ├─ 軸 2 serve helper（用 socket/tcp 接入 → 長駐託管）
   └─ 軸 4 狀態託管（用檔案/dir → 限定 4 標準目錄 + 語意）
```
軸 2 設施建在軸 1 之上，與軸 4 同構。API 形狀（serve 進入點長相、與軸 1 傳輸如何共用）**留待回頭重做**。

---

## 開放問題 / 後續輪次

- 軸 2 設施 API 形狀（serve 進入點、與軸 1 傳輸/軸 5 singleton 的分層）——待 impl 集中重做設計。

---

## Round 4（✅ 複查確認，defs 重新思考 2026-06-27）

對照 `defs_review/axis_2_lifecycle.md`。結論：**Round 3 的 `bool persistent` 核心已對，不動**
（review 自證預設極性 false=one_shot 正確、無 hazard；是不可由他軸推導的二元本質界線、無冗餘欄可砍）。
只做兩件事：

### 1. 流動模式正式判給軸 1，軸 2 不收（止住跨軸流浪）

review 最尖一條：streaming/batch/interactive 在軸 1、軸 2 之間互相推諉、無正式家。裁決——
**它是「某通道的資料怎麼遞送」＝通道（entry）屬性，不是 process 生命週期屬性**。
一個 one_shot process 照樣能串流輸出；從 process 角度仍 one_shot。權威佐證：lib_spec 把 interactive
標在 stdin、streaming 標在 stdout（都掛在通道上）。

→ **流動模式 home = 軸 1 `entry.extra["mode"]`（Round 13 #4 已就位）；軸 2 正式聲明「不承載流動模式」。**
此題 hub-independent，現在閉合，不再以 extra 在兩軸間暫存漂流。

### 2. 變體描述留 extra，不結構化

常駐子型（lazy/warm 首呼啟動、eager daemon、periodic 定時）、detached/fire-and-forget——
一律留 `extra` 自由描述，**不升 opt 欄位**。依「後續別管 hub」原則（2026-06-27 使用者定），
不再為 hub 排程是否需要區分常駐子型而糾結；要結構化時自有需求逼出，現在不預設。

### 型別（定案不變；2026-06-28 內聯進 Meta）

```cpp
struct Meta {
  // ...
  bool persistent = false;   // 軸 2：自我終止 vs 顯式終止
  // ...
  Extra extra;               // 常駐子型 / detached 等變體描述（全軸共用，非軸 2 專屬）
};                           // 流動模式不在此，歸軸 1
```
