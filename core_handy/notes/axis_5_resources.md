# 軸 5：`resources`（資源特性）

> 狀態：✅ 定案（Round 2）。第一個「自由 key-value 袋」本質的軸 → 三層通則首次完整套用。
> 第一個「無最小必須、純推薦+額外」的軸。

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`resources` 僅驗證為 dict，自由 key-value）、
`core_nature/lib_spec.md §4`、`core_nature/axis_spec.md §4`。

`resources` ＝ 對**計算資源的消耗／佔用（含時間）**的宣告，供 Function Hub 排程決策。
自由 key-value：**預定義 key 有規範 value 格式**（hub 可解析）；**自定義 key** value 不限（hub 讀得到、未必懂）。

**資源生命週期維度**（axis_spec §4 Phase 1）：種類 / 啟動時申請 / 執行中動態 / 結束後釋放 / 持續佔用(idle)。

### 預定義 key（lib_spec §4）

| key | 字串形式 | 物件形式 | 備註 |
|---|---|---|---|
| `memory` | `"4gb"`（＝峰值） | `{startup, peak, idle}` 皆選填 | `idle` 僅 `persistent` 有意義 |
| `gpu` | `true`（需 GPU） | `{vram: "6gb"}` | |
| `time` | `"30s"`（預期） | `{expected, max}` | |
| `disk` | `"500mb"` | — | 執行期暫用、結束清除；持久資料歸軸 4 `data` |
| `network` | `true` | — | 是否需網路 |

- 單位：容量 `b/kb/mb/gb`、時間 `ms/s/m/h`。
- 自定義 key 範例：`llm_entry: true`、`db: {type: postgres}`、`render_server: {port: 7860}`。
- **CPU**：Phase 1 分析提到，但**無預定義 key** → 要表達就走自定義（extra）。
- **預設**：缺席＝無任何資源宣告。

---

## Round 1（提案）

### def/impl 定位
- **描述面（→ defs）**：資源足跡宣告，給 hub 排程讀。本軸的濃縮對象。
- **設施面（→ impl）**：資源治理——rate-meter / consume-rate 計量 / 限流 / 配額（對應 Python 側
  `llm_entry_manager` 的 `RateMeter`，集中管 token/金錢/GPU）。**僅定位，待之後 impl 集中重做設計。**

### 三層 ↔ 三欄位（本軸的關鍵濃縮）
資源天生開放（自定義 key 無窮）→ **不可能全固定欄位**。三層恰好對位：

| 定義層 | 內容 | 實作 |
|---|---|---|
| **最小必須（固定）** | **無**——resources 整體可缺席 | （無固定欄位） |
| **推薦（opt）** | 5 個預定義 key：memory/gpu/time/disk/network | `std::optional<具體型別>` |
| **額外（extra）** | 自定義依賴：llm_entry/db/render_server/cpu… | `optional<map<string,string>>` |

→ **軸 5 是第一個「無最小必須、純推薦+額外」的軸**。

### 各預定義 key 的子型別（D2）
沿用軸 1 D2 慣例：**字串形式＝沒帶細節的物件形式** → 統一成 struct，消滅 union。

```cpp
struct Memory {                                  // "4gb" ＝ 只給 peak
  std::optional<std::string> startup;            // 啟動常駐
  std::optional<std::string> peak;               // 執行峰值
  std::optional<std::string> idle;               // 僅 persistent 有意義
};
struct Gpu    { std::optional<std::string> vram; };   // 出現即＝需 GPU；vram 選填
struct Time   { std::optional<std::string> expected, max; };

struct Resources {
  // 推薦（opt 欄位）：預定義資源，結構化、hub 可解析
  std::optional<Memory>      memory;
  std::optional<Gpu>         gpu;        // 有值＝需 GPU
  std::optional<Time>        time;
  std::optional<std::string> disk;       // "500mb"
  std::optional<bool>        network;    // 是否需網路
  // 額外（extra）：自定義資源依賴（llm_entry/db/cpu…），壓平成字串
  std::optional<std::map<std::string, std::string>> extra;
};
```

### 判斷點（待你拍）
- **D1 結構**：無固定欄位、5 opt 預定義 key、extra 收自定義。採？
- **D2 子型別**：memory/gpu/time 各做 struct（字串形式折成 struct）；disk/network 用 scalar。採？
- **D3 值型別**：容量/時間值**先當 `std::string`**（"4gb"/"30s"），不解析成量綱（同軸 1 rate/chunk 慣例）。採？
- **D4 gpu/network presence 語意**：`optional<Gpu>` 有值＝需 GPU；`optional<bool> network` 有值才宣告。採？
- **D5 cpu**：維持「無預定義 key、走 extra」，還是升一個 opt 欄位？（權威無 CPU 預定義，傾向維持 extra）

---

---

## Round 2（✅ 拍板）

| 決策 | 選擇 | 說明 |
|---|---|---|
| **D1 結構** | ✅ 採 | 無固定欄位、5+2 opt 預定義 key、extra 收自定義。 |
| **D2 子型別** | ✅ 採 | memory/gpu/time（+cpu/network）各做 struct（字串形式折成 struct，消 union）；disk 用 scalar。 |
| **D3 值型別** | ✅ 採（+network 修正） | 容量/時間值先當 `std::string`。**但 network 不只 bool → 需帶流量計入**。 |
| **D4 presence 語意** | ✅ 採 | `optional<Sub>` 有值＝需該資源；細節欄位選填。 |
| **D5 cpu** | ✅ **升 opt 欄位** | 權威無 CPU 預定義，**本線刻意升格**為 opt（divergence，記明）。 |

### 兩個與 Round 1 的差異
1. **network 升 struct**：`optional<bool>` → `optional<Network>{ traffic }`。有值＝需網路；`traffic` ＝流量計入
   （與 gpu/vram 同構，餵 impl 層的 consume-rate 計量）。
2. **cpu 升 opt**：新增 `optional<Cpu>{ cores }`。**權威無此預定義 key**，是本 C++ 線的刻意擴充（記明分歧）。

### 型別草圖（定案）

```cpp
struct Memory  { std::optional<std::string> startup, peak, idle; };  // "4gb"＝只給 peak；idle 僅 persistent
struct Gpu     { std::optional<std::string> vram; };                 // 有值＝需 GPU
struct Time    { std::optional<std::string> expected, max; };
struct Network { std::optional<std::string> traffic; };              // 有值＝需網路；traffic＝流量計入

struct Resources {
  // 推薦（opt）：預定義資源，hub 可解析
  std::optional<Memory>      memory;
  std::optional<std::string> cpu;        // 〔本線新增，權威無〕平行度提示 e.g. "4"；缺席＝1（單值→scalar）
  std::optional<Gpu>         gpu;
  std::optional<Time>        time;
  std::optional<std::string> disk;       // "500mb"
  std::optional<Network>     network;
  // 額外（extra）：自定義資源依賴（llm_entry/db/render_server…），壓平成字串
  std::optional<std::map<std::string, std::string>> extra;
};
```

> `cpu` 是**單值資源**→ 用 scalar `optional<string>`，與 disk 同類，不做 struct。
> **為何只計核心數、不計占用 %**：CPU 占用 % 本質是「強度 × 時長」，而時長已由 **time 軸**承載，
> 再宣告 % 會與 time 軸**重疊／糾纏**。核心數是與 time **正交**的量，故 cpu 只取核心數。
> **語意收窄為「平行度提示」（2026-06-24，/btw 複查）**：「CPU 能不能跑」會被 OS 排程器抽象掉
> （切時間片、不宣告也能跑）→ 對「能否執行」無意義。但 `resources` 受眾是 **hub 排程器**，
> 它要回答 OS 不管的「這程式能吃多少平行度」以**避免超賣（oversubscription）**。故 cpu 的語意是
> **平行度上限提示**（thread pool / `-j N` / batch 平行度），**缺席＝1（單執行緒不必宣告）**，
> 而非「我要佔用幾顆」。**核心數正是那個「OS 不抽象掉、且與 time 正交」的維度**。
> ✅ **保留 `cpu`（2026-06-24 拍板）**，語意＝「平行度提示」。(原「保留 vs 拿掉」待決已定為保留。)
>
> **正名：cpu 指「(A) 內部平行度」，不是「(B) 並發安全性」**——兩種「平行度」要分清：
> - **(A) 內部平行度**＝這程式自己開幾條 worker（thread pool/`-j N`/batch）＝**消費量提示**，
>   與 memory/gpu 同性質、同受眾（hub 容量規劃）。**`cpu` key 就是這個。** 缺席＝1。
> - **(B) 並發安全性**＝能否同時跑多份、hub 能否 fan-out ＝執行**結構**，
>   **已被軸 3 `state` 隱含**（stateless⇒可安全平行；stateful_external⇒要小心）。不歸 resources、不需新欄位。
> 兩種讀法都不踩「開新軸」紅線（`axis_spec.md §9` 鐵則：新軸只在「無既有軸能隱含」時才成立）。
> 與權威的分歧（記明）：(1) 新增 `cpu` 預定義（只計核心數）；(2) `network` 由 bool 升為帶 `traffic` 的 struct。

---

## 開放問題 / 後續輪次

- 設施面（rate-meter/限流/配額）API ——待 impl 集中重做設計（network.traffic / cpu.cores 是其輸入）。
- `idle` 僅對 persistent 有意義，是否要型別層強制（跨軸 2）——傾向不強制，文件約束即可。
