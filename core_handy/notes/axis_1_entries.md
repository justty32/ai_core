# 軸 1：`entries`（I/O 出入口）

> 狀態：✅ 定案（Round 12，**三張開放整數碼表，supersede Round 11**）。
> Entry = `unsigned direction=0` + `unsigned content=0` + `unsigned access=0` + `optional<map<string,string>> extra`。
> 仍開放：流動模式 / 傳輸身分的正式家，屬「別的軸 / hub 階段」的事，不阻擋 entries。

> ⚠️ Round 1–9 是 enum 路線、Round 10–11 是扁平 bool 路線（皆保留供回溯）；**最終定案以 Round 12 為準**。

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

## Round 1（提案）

### 濃縮判斷（C++ 比 Python 鬆散 dict 更嚴謹之處）

核心原則：**讓非法狀態無法被表達**。

- **D1（頭條，待拍板）**：`able_in:bool, able_out:bool` → 改用 `Direction { In, Out, InOut }` enum。
  - 理由：兩個獨立 bool 有 4 組合，`(false,false)`＝「既不收也不吐」是廢狀態，Python 不擋；三值 enum 消滅它。
  - 代價：與 JSON wire format（兩個 bool）分歧 → 須在序列化邊界還原。
  - 我的建議：**採用**（濃縮 demo 重設計本質）。
- **D2（待確認）**：`mode`/`type` 的「string 或 object」union → 統一成「enum 判別子 + 選填細節」一個 struct。
  - 理由：string 形式不過是「沒帶細節的 object 形式」；統一後 union 消失，更嚴謹也更簡單。
  - 我的建議：**採用**。
- **D3（待確認）**：`channel_constraint` → `std::optional<ChannelConstraint>`，enum 目前只有 `Stable`（缺席＝無限制）。
  - 我的建議：採用（比 bool 利於未來擴充）。
- **D4（待確認）**：必填欄位怎麼強制——若採 D1 的 `Direction`（無預設、必須給），順帶解掉 designated
  initializer 漏寫 bool 靜默變 false 的問題。
- **D5（待確認）**：預設值（缺席→stdio）用 factory 表達，如 `Entry::stdio()`。

### 提案型別草圖（採 D1+D2+D3）

```cpp
namespace ac {

enum class FlowKind { Batch, Streaming, Interactive };
struct Mode {
  FlowKind kind = FlowKind::Batch;
  std::optional<std::string> rate;        // 僅 streaming："20b/s"
  std::optional<std::string> chunk_size;  // "4kb"
  std::optional<std::string> interval;    // "500ms"
};

enum class ContentKind { Text, Binary };
struct ContentType {
  ContentKind base = ContentKind::Text;
  std::optional<std::string> encoding;    // text："utf-8"
  std::optional<std::string> mime;        // binary："image/png"
};

enum class ChannelConstraint { Stable };

struct TerminalBinding {
  std::string argflag;                     // "--input"
  std::optional<std::string> def;          // 預設接線："stdin"
};

enum class Direction { In, Out, InOut };

struct Entry {
  Direction dir;                           // 必填、無預設
  Mode mode{};
  ContentType type{};
  std::optional<ChannelConstraint> channel_constraint;
  std::optional<TerminalBinding> terminal_binding;
};

using Entries = std::map<std::string, Entry>;
}
```

### 待使用者回覆的問題

1. D1 採不採用（`Direction` 取代兩個 bool）？— 最大分歧點，先拍這個。
2. D2 / D3 確認？
3. 序列化：demo 要不要同時做「輸出 `--metadata` JSON」？（影響 D1 的 wire-format 還原）
4. `rate`/`chunk_size` 先當 `std::string`，還是解析成型別量？（我傾向 v0 先 string）

---

## Round 2（三場景窮舉 → 通道的角色與正交性質）

把 `entries` 的本質「函數對外的通道 + 通道性質」攤在三個經典場景上窮舉。

### A. C 語言函數

- **in-band（走簽名）**：傳值參數(in)、返回值(out)、指標參數(in/out/in-out)、二級指標 `T**`
  (out+所有權移轉)、上下文 handle `ctx*`(in-out)、函式指標/callback(out→in，控制反轉/非同步雛形)、variadic。
- **out-of-band/環境**：全域 / static / TLS 變數(in+out)、`errno`/TLS 錯誤(out，狀態側通道)、堆積/配置器(所有權)。
- **指標的「性質鏈」其實是通道的正交性質**：純參考 → `const`(唯讀) → 可寫；再疊所有權(借用 vs 移轉)、
  可空性、長度伴隨參數(`buf,len`)、aliasing(`restrict`)。

### B. Shell 程式呼叫

- **in-band**：argv(位置/`--flag`/`-k`，可直接帶資料或帶路徑)、stdin、stdout、stderr(診斷側通道)、
  exit code(out，極窄狀態通道)、額外 fd(`3<`/`4>`)。
- **indirection（引數命名通道，檔案才是通道，對應 C 指標）**：檔案路徑讀(in)/寫(out)/in-place(in-out)、
  目錄、特殊檔 `/dev/null`(sink)、`/dev/zero`/`/dev/random`(source)、`/dev/fd/N`/`/dev/shm`、
  FIFO/匿名 pipe(streaming，斷裂＝SIGPIPE)、`-` 慣例。
- **環境/ambient**：環境變數(in；export 給子程序＝out)、cwd、signals(SIGINT/SIGTERM/SIGHUP-reload，控制側通道)、
  umask/ulimit/locale/TZ、lock/pid 檔。

### C. Server API 呼叫

- **請求(in)**：method(派發鍵)、URL path+path params、query string、headers(含 `Authorization`＝身分通道、
  內容協商)、cookies(session/ambient)、request body(batch 或 client-streaming)。
- **回應(out)**：status code(≈ exit code)、response headers(`Location`/`Set-Cookie`＝寫客戶端狀態/`ETag`/rate-limit)、
  response body(batch 或 server-streaming：chunked/SSE)。
- **串流/雙工**：SSE、WebSocket、gRPC client/server/bidi streaming、long-poll。
- **非同步/out-of-band**：Webhook callback URL(out→in 事後回呼，**＝ C 的 callback/函式指標**)、
  job-id+輪詢(`202`→poll `/jobs/{id}`，≈ future/handle)、訊息佇列/event bus。
- **控制/meta（牽動他軸）**：`Idempotency-Key`(→guarantee)、pagination cursor(out 變下次 in，有狀態迭代)、
  `ETag`/`If-None-Match`(快取/條件)、rate-limit headers(→resources)。

### 收斂：重複出現的角色（跨三場景同構）

| 角色 | C | shell | server |
|---|---|---|---|
| 主輸入 | 參數 | stdin / 檔案 | request body |
| 主輸出 | 返回值 | stdout / 檔案 | response body |
| 設定/控制 | 旗標式參數 | argv flags + env | query + headers |
| 狀態/錯誤 | 返回碼 + errno | exit code + stderr | status code + error body |
| 身分/權限 | — | uid(ambient) | auth header + cookie |
| ambient 狀態 | 全域 | env + cwd | cookie + session |
| 非同步續傳 | callback 指標 | 背景 job/signal | webhook + job-poll + queue |

### 收斂：單一通道的 8 個正交性質（候選）

1. 方向 in/out/in-out
2. 定址 by-value vs by-reference(indirection：指標/檔案路徑/URL/job-id)
3. 可變性＋所有權 const借用 / 可變借用 / 移轉
4. 基數＋流動 單值 / batch / stream(push｜pull)
5. 內容 text/binary/結構化
6. 頻帶 in-band(走簽名) vs ambient(out-of-band)
7. 同步性 同步 vs 非同步
8. 可靠度 穩定 vs 易斷

> 對照舊 `entries`：已涵蓋 1/4/5/8，定址(2) 只在 `terminal_binding` 沾邊；完全未模型化 2(by-val/ref)、3(可變/所有權)、6(頻帶)、7(同步)。

---

## Round 3（✅ 範圍拍板：entries 只模型化四個性質）

使用者決定：**每個通道最基本一定帶、且只模型化這四個性質**（四個皆必填、無預設）：

| 屬性 | 值域 | 對應 Round2 編號 | 一句話 |
|---|---|---|---|
| **方向** | In / Out / InOut | 1 | 資料往哪流 |
| **可變性** | ReadOnly / Mutable / Owned | 3 | 唯讀借用 / 可寫 / 所有權移轉 |
| **內容種類** | Text / Binary | 5 | 文字 / 二進位 |
| **可靠度** | Stable / Lossy | 8 | 穩定 / 錯誤率高（pipe 易斷、網路會掉） |

**被刻意排除（連帶後果，待確認其去向）：**
- `mode`（batch/streaming/interactive，正交性質 4）— 不進 entries。若有家，較可能歸 lifecycle 或另立流動軸。
- `terminal_binding`（CLI 接線，定址 2）— 不算通道性質；本就只在 terminal 環境有意義，移出更乾淨。
- 內容 object 形式（`encoding`/`mime`）— 砍掉，只留 text/binary 二值。
- 定址(2)、頻帶(6)、同步性(7) — 不納入。

**敲定的語意決定：**
- 可靠度是**描述**（這通道本身的錯誤特性），**不是要求**（不是「我要求通道至少多穩」）。
  → 推翻舊 `channel_constraint`「品質下限」的要求語意。

**待確認的小邊：**
- 方向 vs 可變性微妙重疊（Out≈Mutable、In≈ReadOnly）；可變性真正額外帶來的是「所有權移轉/consume」與「in-out 原地改」。日後或需再收斂避免兩 enum 表達同一件事。
- **狀態/數值通道**（exit code / HTTP status / errno 等小整數）套「內容＝text/binary」會繃：歸 binary？補第三值 numeric/scalar？還是不當 entry 通道而歸他軸？— 標記，未決。

### 重塑後的型別草圖（四性質版）

```cpp
namespace ac {

enum class Direction   { In, Out, InOut };
enum class Mutability  { ReadOnly, Mutable, Owned };
enum class ContentKind { Text, Binary };
enum class Reliability { Stable, Lossy };

struct Entry {
  Direction   dir;
  Mutability  mut;
  ContentKind content;
  Reliability reliability;
};

using Entries = std::map<std::string, Entry>;
}
```

---

## Round 4（✅ 再砍：可靠度也移除 → 三個必要性質）

使用者決定：**可靠度非必要，移除**。理由：它較像通道的「附帶觀察」，不是每個通道都得宣告的本質屬性。
若日後要表達「pipe 易斷/網路掉包」，再當選填或另立去處。

**最終必要性質（三個，皆必填、無預設）：**

| 屬性 | 值域 | 一句話 |
|---|---|---|
| **方向** | In / Out / InOut | 資料往哪流 |
| **可變性** | ReadOnly / Mutable / Owned | 唯讀借用 / 可寫 / 所有權移轉 |
| **內容種類** | Text / Binary | 文字 / 二進位 |

### 重塑後的型別草圖（三性質版）

```cpp
namespace ac {

enum class Direction   { In, Out, InOut };
enum class Mutability  { ReadOnly, Mutable, Owned };
enum class ContentKind { Text, Binary };

struct Entry {
  Direction   dir;
  Mutability  mut;
  ContentKind content;
};

using Entries = std::map<std::string, Entry>;
}
```

---

## Round 5（✅ 再砍：可變性也移除 → 兩個必要性質）

使用者決定：**可變性非必要，移除**。理由：它與方向高度重疊（Out≈Mutable、In≈ReadOnly，Round 3/4 已標記），
方向已扛走大部分資訊；剩下的「所有權移轉/consume」「in-place 原地改」細微差別暫不表達，需要時再回來。

**最終必要性質（兩個，皆必填、無預設）：**

| 屬性 | 值域 | 一句話 |
|---|---|---|
| **方向** | In / Out / InOut | 資料往哪流 |
| **內容種類** | Text / Binary | 文字 / 二進位 |

### 重塑後的型別草圖（兩性質版）

```cpp
namespace ac {

enum class Direction   { In, Out, InOut };
enum class ContentKind { Text, Binary };

struct Entry {
  Direction   dir;
  ContentKind content;
};

using Entries = std::map<std::string, Entry>;
}
```

---

## Round 6（✅ 收斂：兩層模型 = 兩必填 + 兩選填）

使用者修正 Round 4/5：可變性與可靠度**不是砍掉，是降級成選填**。最終 entry 性質分兩層：

| 層級 | 屬性 | 值域 | 一句話 |
|---|---|---|---|
| **必填** | **方向** | In / Out / InOut | 資料往哪流 |
| | **內容種類** | Text / Binary | 文字 / 二進位 |
| **選填** | **可變性** | ReadOnly / Mutable / Owned | 唯讀借用 / 可寫 / 所有權移轉；缺席＝不在意 |
| | **可靠度** | Stable / Lossy | 穩定 / 易斷；缺席＝不在意 |

語意：方向＋內容是「一個通道至少要說清楚的兩件事」；可變性、可靠度是「多數通道不必管，
少數場景（in-place 改檔、易斷 pipe）想標就標」。`std::optional` 表達「缺席＝沒宣告/不在意」。

### 重塑後的型別草圖（兩必填 + 兩選填）

```cpp
namespace ac {

enum class Direction   { In, Out, InOut };
enum class ContentKind { Text, Binary };
enum class Mutability  { ReadOnly, Mutable, Owned };
enum class Reliability { Stable, Lossy };

struct Entry {
  Direction   dir;                          // 必填
  ContentKind content;                      // 必填
  std::optional<Mutability>  mut;           // 選填，缺席＝不在意
  std::optional<Reliability> reliability;   // 選填，缺席＝不在意
};

using Entries = std::map<std::string, Entry>;
}
```

> 可靠度語意維持 Round 3 定案：**描述**（通道本身錯誤特性），非「要求」。

---

## Round 7（✅ 內容種類改自由字串，溶掉數值通道邊）

使用者決定：**內容種類是開放集合**（text/binary/json/png/int/protobuf… 無窮），enum 注定要不斷追加 →
改成 **`std::string`**。連帶**溶掉 Round 3 標記的「狀態碼/數值通道」邊**：整數通道寫 `"int"`、
JSON 寫 `"application/json"`，不必再為 enum 值域煩惱。

> **設計張力備忘**：這違反「讓非法狀態無法被表達」（自由字串什麼都能塞），但成立——內容型別本質開放，
> 強行 enum 是徒勞。同舊 Python 設計（type 為 str / object）。

**待定（Round 7 衍生）**：內容字串要不要訂「建議慣例」？候選：**MIME type**（`text/plain`、
`application/json`、`image/png`、`application/octet-stream`…），借現成開放詞彙＝不重造輪子、跨工具好對齊；
仍是自由字串、不強制。— 待使用者拍板採不採。

### 重塑後的型別草圖（方向 enum + 內容 string + 兩選填）

```cpp
namespace ac {

enum class Direction   { In, Out, InOut };
enum class Mutability  { ReadOnly, Mutable, Owned };
enum class Reliability { Stable, Lossy };

struct Entry {
  Direction   dir;                          // 必填
  std::string content;                      // 必填，自由字串（建議 MIME 慣例，待定）
  std::optional<Mutability>  mut;           // 選填，缺席＝不在意
  std::optional<Reliability> reliability;   // 選填，缺席＝不在意
};

using Entries = std::map<std::string, Entry>;
}
```

---

## Round 8（✅ entries 核心定案）

- 內容字串**不採** MIME 慣例 → 保持純自由字串，呼叫方自行約定。
- 至此 `entries` 的 entry 結構定案：

```cpp
namespace ac {

enum class Direction   { In, Out, InOut };
enum class Mutability  { ReadOnly, Mutable, Owned };
enum class Reliability { Stable, Lossy };

struct Entry {
  Direction   dir;                          // 必填
  std::string content;                      // 必填，自由字串（無建議慣例）
  std::optional<Mutability>  mut;           // 選填，缺席＝不在意
  std::optional<Reliability> reliability;   // 選填，缺席＝不在意
};

using Entries = std::map<std::string, Entry>;
}
```

---

## Round 9（✅ entries 無預設值）

使用者決定：**`entries` 沒有預設值**。推翻舊設計「缺席＝單一 stdio entry（batch/text/雙向）」。
要有通道就明確宣告，不靠隱含預設 — 與「不藏意圖」一致。

→ 至此 `entries` 軸**完全定案**：entry 結構（Round 8）+ 無預設值（Round 9）。

---

## Round 10（✅ 重新設計：扁平 bool + extra map，supersede Round 8/9）

使用者推翻 enum 路線，改扁平設計。演化：先提
`bool able_in, able_out, text, readonly, reliable, optional<map<string,string>>`，
再砍 `able_in`/`able_out`/`reliable`。**最終定案：**

```cpp
struct Entry {
  bool text;       // 人類可讀（涵蓋 text 與 json，必 utf-8）vs 二進位
  bool readonly;   // 唯讀 vs 可寫
  std::optional<std::map<std::string, std::string>> extra;  // 輔助描述/擴充，缺席＝無
};

using Entries = std::map<std::string, Entry>;  // 具名通道 → entry
```

**欄位語意：**
- `text`：問的不是「格式」而是「人讀不讀得懂」——text 與 json 都 `true` 且保證 utf-8；`false`＝二進位。
- `readonly`：函數是否**不改動**該通道背後的資源（讀檔 true；寫檔/in-place false）。同時隱含方向味道
  （readonly≈輸入、!readonly≈輸出）。
- `extra`：自由 key-value，承載先前砍掉的細節（mime/encoding/rate…）與任意擴充。

**relative to enum 路線（Round 1–9）的取捨：**
- Direction enum → 刪除（able_in/able_out 也刪）；方向不再結構化，由 `readonly` 隱含承載，要精確就丟 `extra`。
- 可變性（3 值 enum）→ 壓成 `bool readonly`（丟失 Owned/所有權移轉的區分）。
- 可靠度 → **刪除**（連 bool 都不留）。
- 內容自由字串 → 壓成 `bool text`（人類可讀 vs 二進位）。
- 無預設值的決定（Round 9）：bool 預設值未明定 → 待定（見開放問題）。

**小提醒（不阻擋）**：able_in/able_out 拿掉後，沒有「既不收也不吐」的廢狀態問題了（因為不再描述方向）。

---

## Round 11（✅ 最終：rename + 預設值）

- `text` 預設 `false`（＝二進位）。
- `readonly` **改名 `writable`**、預設 `false`（＝唯讀；極性翻轉但語意同：預設不可寫）。

**最終定案結構：**

```cpp
struct Entry {
  bool text = false;       // 人類可讀（涵蓋 text 與 json，必 utf-8）vs 二進位；預設二進位
  bool writable = false;   // 可寫 vs 唯讀；預設唯讀
  std::optional<std::map<std::string, std::string>> extra;  // 輔助描述/擴充，缺席＝無
};

using Entries = std::map<std::string, Entry>;  // 具名通道 → entry
```

---

## Round 12（✅ 最終：bool → 三張開放整數碼表，supersede Round 10/11）

回應 `defs_review/axis_1_entries.md` 的審查。使用者決定：**用開放整數碼表取代 bool**，
讓 bool 的 0/1 成為碼表子集（同軸 6 `unsigned level`）——動機是**向前擴充相容**：
未來加碼 2/3/4… 不破壞只懂 0/1 的舊讀者。三個正交性質各一張表，全 `unsigned`、zero-init 預設、具名常數補可讀性。

### 三張開放碼表（拍板）

| 欄位 | 碼 | 標準值 | 預設語意 |
|---|---|---|---|
| `direction` | 0 / 1 / 2 / ≥3 | in / out / in_out / 擴充 | 0=in（純輸入；與 access=0 readonly 相配、不衝突） |
| `content`   | 0 / 1 / ≥2 | binary / text / 擴充(int/json…) | 0=binary（**沿用舊 bool text 極性 false=binary，選相容而非修預設**） |
| `access`    | 0 / 1 / ≥2 | readonly / writable / 擴充(owned/consume…) | 0=readonly（保守端） |

### 使用者拍板的三個決定
1. **content 極性 = 0=binary**：選「跟舊 bool 值相容」而非「修預設極性」。忘填→binary 的 hazard 接受。
2. **改名**：`text`→`content`、`writable`→`access`（多值碼表，名實相符）。
3. **方向碼表**：`in=0`、`out=1`、`in_out=2`；≥3 擴充。
   （初版指定 `in_out=0`，後改 `in=0`：避免 zero-init 的 `in_out` 與 `access=0 readonly` 字面衝突。）

### 解掉的審查發現
- **#2 方向回歸**：`direction` 與 `access` **分立** → 方向(in/out/inout) 與可變性(讀/寫) 正式解耦。
  **in-place = `direction:in_out` + `access:writable`**，Round 5 標記「in-place 暫不表達」的缺口補上。
- **#1 數值通道**：exit code/int/json = `content` 碼 ≥2 → Round 7「自由字串 content」以整數碼表還魂
  （與軸 6/9 同語言：整數碼＋具名常數），Round 11 打回的回歸再次解掉。
- **#5 預設極性**：依使用者選擇，content 維持 0=binary（相容優先）；不修極性。

### 型別草圖（定案）

```cpp
struct Entry {
  unsigned direction = 0;  // 0=in(預設) 1=out 2=in_out      ≥3 擴充
  unsigned content   = 0;  // 0=binary(預設) 1=text          ≥2 擴充(int/json…)
  unsigned access    = 0;  // 0=readonly(預設) 1=writable     ≥2 擴充(owned/consume…)
  std::optional<std::map<std::string, std::string>> extra;
};
using Entries = std::map<std::string, Entry>;

namespace dir   { constexpr unsigned in = 0, out = 1, in_out = 2; }
namespace ctype { constexpr unsigned binary = 0, text = 1; }
namespace acc   { constexpr unsigned readonly = 0, writable = 1; }
```

### 與 Round 10/11 的取捨
Round 10/11 砍成兩 bool，丟了方向與數值通道；Round 12 用三張開放碼表補回，
但保持「扁平、無 union、序列化為整數」的 KISS——三個 `unsigned` 在 `--metadata` 直接輸出為整數。

### 全 zero-init 的語意（已校正）
全 zero-init 的 Entry ＝ `in × binary × readonly` ＝「唯讀的二進位輸入通道」，方向×可變性**自洽**
（純輸入本就唯讀），且是最小常見 case。**這正是把 `direction` 預設從 `in_out` 改成 `in` 的理由**：
避免 zero-init 的 `in_out × readonly`（雙向卻唯讀）字面衝突。

### 仍延後（hub 階段）
- 流動模式（batch/streaming/interactive）的正式家——見軸 2 審查；現以 extra 暫存。
- 傳輸身分（檔案/stdin/tcp）——組合配對需求，留 hub；現以 map-key + extra 暫存。

---

## def/impl 定位（2026-06-24 複查）

軸 1 **兩面都有**（前一版誤判為「純描述、無設施」，已更正）：

### 描述面（→ defs）✅ 已定案
`Entry{ bool text, bool writable, optional<map> extra }`（Round 11）。三槽：有固定、無 opt、有 extra。
給 hub 讀、序列化進 `--metadata`：說清楚「這通道人讀不讀得懂、改不改它」。

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
