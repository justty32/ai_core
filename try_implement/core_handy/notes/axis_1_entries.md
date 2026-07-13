# 軸 1：`entries`（I/O 出入口）

> ⚠️ 歷史討論記錄（2026-06-28 收斂）。**事實基準在 `../defs/axes.hpp`**（本軸已驗證對齊）。本檔只留最終拍板（**Round 14**）與現況脈絡；Round 13 留作前身脈絡，被取代的 Round 1–12 見 [`archived/axis_1_history.md`](archived/axis_1_history.md)。

> 狀態：✅ 定案（**Round 14，Unix 統一 I/O：三碼表 `direction`/`flow`/`content`，supersede Round 13**）。
> Entry = `unsigned direction` + `unsigned flow`(0=batch/1=streaming) + `unsigned content`。
> 「一切都是檔案」：transport 種類不進描述（退化成 runtime 位址 scheme，作者與 hub 一律用同一對 read/write）；
> 描述只留 read/write 統一介面藏不掉的 `flow`。mutation→軸 3、transport/位址→runtime。

> ⚠️ Round 1–9 enum 路線、Round 10–11 扁平 bool、Round 12 三碼表(含 access)、Round 13 兩碼表——皆已 supersede；**最終定案以 Round 14 為準**。Round 1–12 見 [`archived/axis_1_history.md`](archived/axis_1_history.md)。

> 🔄 2026-06-28 再收斂：各軸 extra 已上收為單一 `../defs/axes.hpp` 的 `Meta::extra`；本軸 `Entry` 無 extra。

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py` 的 `_validate_entries` / `_validate_entry_mode` / `_validate_entry_type`、
`docs/spec/lib_spec.md §1`、`docs/spec/axis_spec.md §1`。

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

## Round 13（⛔ 已被 Round 14 取代，supersede Round 12）— 砍 access，剩兩欄

> 註：Round 13 是 Round 14 的直接前身（兩碼表 direction/content）。保留作脈絡；現況見檔尾 Round 14。

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

---

## Round 14（✅ 定案，supersede Round 13）— Unix 統一 I/O：補 flow、transport 出描述

使用者定錨：**學 Unix 哲學「一切都是檔案」**——檔案 / pipe / socket / shm / tcp 開出來都是 fd，
全用同一對 `read`/`write`。作者寫 I/O、hub 串接，都只認 read/write。受眾確立為三方：
**entries 是自我描述、給作者寫 I/O 方便、也給 hub 串接方便。**

### 兩個推論

1. **transport 種類退出描述**（推翻「T2/T3 把 transport 當欄位」的前一版傾向）。
   既然存取統一成 read/write，作者與 hub 都不需要「它是 tcp 還是檔案」來操作——
   transport 退化成 **runtime 的位址 scheme**（`-`=stdio / `path` / `tcp://…` / `shm:…`），
   open 時由 lib 認。**種類(kind) 靜態不入 defs；位址(address) 本就 runtime（`ac::cli::resolve`）。**
2. **補一個 `flow`**：read/write 的統一介面唯一藏不掉的行為差異是 **batch vs streaming**
   （一般檔案可 seek、整塊讀完 vs pipe/socket 順序、逐塊、一次性、可能不終止）。
   這是描述必須留的一維。

### 型別草圖（定案）

```cpp
struct Entry {
  static constexpr unsigned in = 0, out = 1, in_out = 2;  // direction
  static constexpr unsigned batch = 0, streaming = 1;     // flow（≥2 擴充 interactive/duplex）
  static constexpr unsigned binary = 0, text = 1;         // content

  unsigned direction = 0;   // 0=in / 1=out / 2=in_out
  unsigned flow      = 0;   // 0=batch / 1=streaming / ≥2 擴充
  unsigned content   = 0;   // 0=binary / 1=text / ≥2 擴充
};
using Entries = std::map<std::string, Entry>;
```

序列化：`{"stdin":{"direction":0,"flow":0,"content":1}, ...}`。

### 三受眾各取（全靠這三維 + 統一 read/write）

| 受眾 | 怎麼用 |
|---|---|
| 作者 I/O | 看 `flow`：batch → `read_all/write_all(addr)`；streaming → `Channel(addr)` 逐塊。不管 transport，scheme 由 lib 認 |
| hub 串接 | 比 `direction`＋`flow`＋`content`：兩端 streaming → 架 pipe 直連；batch → 經暫存檔；content 判相容 |
| 自我描述 | `--metadata` 直接吐三碼 |

### 決定

- **`content` 留**（access 統一 ≠ 內容語意統一；text/binary 正交、hub 也要）。
- **`flow` 用開放 `unsigned`**（同 direction/content、同 R12 向前相容理由），預設 `0=batch`（zero-init=最常見）。
- 與 impl 的關係：`flow` 直接對上 impl_1_io 的 batch 群（`read_all`）/ stream 群（Channel）分界——
  描述面從此把該分界正當化（先前是 impl 自己浮現的）。

### 合法組合（無廢狀態）

| direction × flow | 例 |
|---|---|
| in × batch | `sort` 讀完才排 |
| in × streaming | `grep` 邊讀邊濾 |
| out × streaming | 持續吐日誌 |
| in_out × batch | `sed -i` 原地改檔 |
| in_out × streaming | 雙工 socket（≥2 擴充） |
