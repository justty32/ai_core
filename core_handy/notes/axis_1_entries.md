# 軸 1：`entries`（I/O 出入口）

> ⚠️ 歷史討論記錄（2026-06-28 收斂）。**事實基準在 `../defs/axes.hpp`**（本軸已驗證對齊）。本檔只留最終拍板（Round 13）與現況脈絡；被取代的 Round 1–12 見 [`archived/axis_1_history.md`](archived/axis_1_history.md)。

> 狀態：✅ 定案（Round 13，**砍掉 access，剩兩碼表＋extra，supersede Round 12**）。
> Entry = `unsigned direction=0` + `unsigned content=0` + `optional<map<string,string>> extra`。
> 仍開放：流動模式 / 傳輸身分 / mutation，屬「別的軸 / hub 階段」的事，不阻擋 entries（PARKED-HUB）。

> ⚠️ Round 1–9 是 enum 路線、Round 10–11 是扁平 bool 路線（皆保留供回溯）；**最終定案以 Round 13 為準**。

> 🔄 **2026-06-28 再收斂：extra 已上收為單一 `../defs/axes.hpp` 的 `Meta::extra`；本軸 `Entry` 移除 extra 欄位。**

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py` 的 `_validate_entries` / `_validate_entry_mode` / `_validate_entry_type`、
`core_nature/lib_spec.md §1`、`core_nature/axis_spec.md §1`。

`entries` 是 §1「I/O 型態」軸的具體化：一個「**語意名稱 → 出入口描述**」的 map，
每個出入口（entry）描述函式的一個 I/O 埠。

| 欄位 | 型別 | 必填 | 語意 |
|---|---|---|---|
| `able_in` | bool | ✅ | 是否接收輸入 |
| `able_out` | bool | ✅ | 是否輸出 |
| `mode` | str \| object | 選填 | 資料流動形式：`batch`/`streaming`/`interactive`；object 形式給 streaming 加速率（`rate`/`chunk_size`/`interval`），`type` 必填 |
| `type` | str \| object | 選填 | 內容格式：`text`/`binary`；object 形式 `base` 必填，加 `encoding`/`mime` |
| `channel_constraint` | str | 選填 | 通道品質下限，**目前只定義 `"stable"`**（缺席＝無限制） |
| `terminal_binding` | object | 選填 | CLI 接線（`argflag`+`default`），僅 terminal 環境有意義；換環境整塊忽略 |

**預設值**：`entries` 缺席 ＝ 單一 stdio entry（batch、text、雙向）。

**Terminal 預定義名稱**（`lib_spec.md` §1 表）：`stdin`(in/batch|interactive/text)、
`stdout`(out/batch|streaming/text)、`stderr`(out/streaming/text，通常不必出現在 entries)。

---

## def/impl 定位（2026-06-24 複查）

軸 1 **兩面都有**（前一版誤判為「純描述、無設施」，已更正）：

### 設施面（→ impl）份量大，**待設計（筆記層）**
lib 要替程式**跨傳輸做真實讀寫**，把「I/O 這件事」統一託管，作者不必為每種通道各寫一套：

| 傳輸 | 設施涵蓋 | 備註 |
|---|---|---|
| 檔案／資料夾 | 依路徑讀寫、建目錄、存在檢查、in-place 改 | 軸 4 狀態託管是此格的**特化**（限定 4 個標準目錄 + 語意） |
| pipe | stdin/stdout、具名 FIFO、SIGPIPE、streaming chunk | 最原始那格靠 OS，但統一介面仍由 lib 包 |
| shared memory | `/dev/shm`、mmap | |
| tcp / http | socket、HTTP client | 對應 Python 側 `lib/call.Http`（urllib 零相依）的 C++ 對應 |

**關鍵關係**：軸 4（狀態託管）⊂ 軸 1 檔案/資料夾設施——軸 4 是「限定在 config/cache/state/data + 語意約束」
的特化。→ 軸 4 設施**可建在軸 1 I/O 設施之上**。

**描述面 vs 設施面的受眾差異**：
- 描述面（text/writable）給 **hub** 看，抽象、傳輸無關。
- 設施面要知道**傳輸身分**（檔案路徑？tcp endpoint？shm 名？）才能真讀寫。傳輸身分**不在 defs 的 Entry**裡
  （Round 3 已把 addressing/terminal_binding 移出描述），它是設施層的輸入。→ 兩者**相關但分屬兩層**。

### 待拍：軸 1 設施 API 形狀（D-IO）
- 統一讀寫介面長相（自由函式 `ac::io::read/write` vs 通道物件 `ac::Channel`）、傳輸如何指定、
  與軸 4 託管設施如何分層共用——**留待 impl 層集中設計**（先做筆記定位，不寫碼）。

## 開放問題 / 後續輪次

- `extra` 的 key 要不要訂建議詞彙（如沿用 entries 砍掉的 mime/encoding/rate）— 待定。
- mode/流動軸的去向（Round 3 排除後待安置；streaming 現以 lifecycle.extra 承載）。
- terminal_binding 移到哪——現判定屬**設施層的傳輸接線**，非描述。

---

## Round 13（✅ 定案，supersede Round 12）— 砍 access，剩兩欄

defs 重新思考（2026-06-27），對照 `defs_review/axis_1_entries.md`。逐項落定：

```cpp
struct Entry {
  unsigned direction = 0;   // 0=in / 1=out / 2=in_out（開放碼表，序列化為整數）
  unsigned content   = 0;   // 0=binary / 1=text / ≥2 擴充（json / status-int…）
};
using Entries = std::map<std::string, Entry>;
```

> 註（2026-06-28）：原 `Entry::extra` 欄位已移除；PARKED-HUB 暫存改走單一 `Meta::extra`（見 `../defs/axes.hpp`）。

對 Round 12 的兩處改動：

1. **`access` 欄整個刪除**（mutation 資訊冗餘於軸 3 `stateful` + 傳輸身分；in-place 靠 `direction=in_out` 已完整表達）。
2. **`direction` 維持 `unsigned` 開放碼表**（不改 enum；序列化直出整數的 KISS 線保留）。`content` 不動。

### 通道角色維：不立

stderr / exit code / http status 暴露出一個「通道角色」維（diagnostic / status / data），**決定不形式化**——
連 `extra["role"]` 都不預留。識別靠兩個免費把手：**通道名**（lib_spec 預定義 stdin/stdout/stderr 語意）＋
**要不要列進 entries**（stderr 通常不列）。真到 hub 要「別把 stderr 接出去」時，那是 hub 紀元的事，現在零預留。

逐 case 驗證收斂形：

| 通道 | direction | content | 備註 |
|---|---|---|---|
| stdin | 0 in | 1 text | |
| stdout | 1 out | 1 text | |
| stderr | 1 out | 1 text | 通常不列 |
| exit code / http status | 1 out | 2 status-int | 格式靠 content；角色不形式化 |
| 讀 config 檔 | 0 in | 0/1 | mutation 跟 transport 走，PARKED-HUB |
| `sed -i` in-place | 2 in_out | 0/1 | in_out 已表達「又讀又寫」 |
| 寫新檔 | 1 out | 0/1 | 「會改檔」= 軸 3 stateful + transport，不在此 |

### PARKED-HUB（不阻擋軸 1，全暫寄 extra）

- **#3 傳輸身分**（stdin/檔案/tcp/shm）：hub 配對 `A.stdout→B.stdin` 才需要。
- **#4 流動模式**（batch/streaming/interactive）：與軸 2 同一懸案連動。
- **mutation**（會不會改後端資源）：通道級跟傳輸身分走、程式級歸軸 3 `stateful`。

三者去留全釘在 review 根源懸案「hub 消費 entries 哪幾維來組合」上——先定受眾再回頭決定升不升固定欄位。
