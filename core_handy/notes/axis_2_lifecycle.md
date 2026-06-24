# 軸 2：`lifecycle`（生命週期）

> 狀態：✅ 定案（Round 3 修正）。`bool persistent`（預設 false=one_shot）+ 統一 `extra`（取代原 `detail`）。

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

```cpp
struct Lifecycle {
  bool persistent = false;                                    // false=one_shot(預設)、true=persistent
  std::optional<std::map<std::string, std::string>> extra;    // 變體描述：lazy/warm/periodic/streaming…
};
```

> singleton 仍歸 resources 軸；streaming＝one_shot 輸出變體，現以 extra 承載（不另立流動軸，需要時再升級）。

---

## def/impl 定位（2026-06-24 複查）

> 設施面**僅定位、不設計 API**。之後會回頭重看全部 impl 筆記、統一重做實作設計。

### 描述面（→ defs）✅ 已定案
`bool persistent = false` + 統一 `extra`（見 Round 3）。給 hub 讀：自我終止 vs 顯式終止。

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
