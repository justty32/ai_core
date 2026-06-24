# 軸 4：`state_dirs`（狀態統一託管設施）

> 狀態：討論中（重定位後 Round 1）。**性質已澄清：軸 4 不是描述性 metadata 欄位，而是「標準實作庫提供的設施」。**
>
> ### 重大重定位（2026-06-24）
> 見 `00_index.md`「兩層區分」。
> - 在**九軸標準定義**裡：程式狀態自管，**軸 4 不存在於描述性九軸**（描述性軸到軸 3 `stateful` 為止）。
> - 在**標準實作庫（core_handy）**裡：軸 4 是**提供給程式選用的設施**——標準目錄慣例（config/cache/state/data）
>   的現成實作（路徑解析、建目錄、JSON 讀寫）。程式把狀態**託管**給它，而不是自己 reimplement。
>
> 因此軸 4 的工作**不是「濃縮一個型別」，而是「設計一組 API」**。
>
> 連帶（仍成立）：軸 3 維持獨立 `bool stateful`，不與軸 4 合併（前一輪 D3 的 (b) 合併案不採）。
>
> 下方「§0 權威定義」沿用；「Round 1（提案）」是**舊的描述性欄位提案**，已被本重定位取代，保留作脈絡。

---

---

## 0. 權威定義（濃縮對象）

來源：`src/ai_core/_core.py`（`_STATE_DIR_VALUES = {config, cache, state, data}`，驗證為 list 且須為子集）、
`core_nature/lib_spec.md §3`、`core_nature/composite_spec.md`「標準狀態目錄慣例」。

`state_dirs` ＝ 當程式遵守 terminal「標準狀態目錄慣例」時，**宣告它實際用到哪些標準目錄**。
允許值是 `{config, cache, state, data}` 的**子集**（array of string）。

四個目錄語意（工作目錄下的標準路徑，可為資料夾或單 `.json` 檔）：

| 目錄 | 語意 | 可否刪除 |
|---|---|---|
| `config` | 程式本身不改的設定，由人類/外部工具寫入 | 視情況（刪後用預設值） |
| `cache` | 執行時間之外可任意刪，程式不依賴其存在 | 可隨時刪 |
| `state` | 程式目前所在的 stage／跨輪次階段位置 | 刪後遺忘進度，可重置 |
| `data` | 累積建造的成果性資料；重要 | 不可隨意刪，需備份 |

> `state` vs `data` 分野：清 `state` 只是忘了做到哪（可重置）；清 `data` 是真失去成果（需備份）。

### 關鍵性質（既有設計已定）

- **資訊性宣告，非強制合約**：目的是讓 hub 了解副作用範圍，不是 enforce。
- **是 §3 `state` 的配套**：複合慣例觸發條件＝`state: "stateful_external"` + terminal。
  → state_dirs 只有在 `stateful` 時才有意義。
- **預設／缺席**：缺席時 hub 無法推斷用哪些目錄，但 `stateful` 本身已足以通知「有副作用」。
- 跨軸牽連（composite_spec）：這四個目錄同時是 §1 file I/O 通道（`channel_constraint: stable`）、
  影響 §5 中斷風險（data 損毀風險最高）、§6 保證（是否冪等/事務）。本軸只負責「用到哪些」。

---

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

## 設施設計（筆記層）〔現況・討論中〕

> 階段約束：**先做筆記定義，不寫 code**（與描述性軸一致）。本節只定義設施的職責與範圍，API 形狀待拍。

### 設施職責（範圍）
把「程式的外部狀態存放」這件事統一託管，免得每個程式各自 reimplement。涵蓋：

| 職責 | 說明 |
|---|---|
| 路徑解析 | 工作目錄下的 `config` / `cache` / `state` / `data`；每個可為資料夾或單 `.json` 檔 |
| 目錄管理 | 按需建目錄、存在性檢查 |
| 讀寫 | JSON 讀寫；單檔須為 JSON 物件，資料夾內子檔格式不限 |
| 語意約束 | `config` 取向唯讀（外部寫入）、`cache` 可隨時丟、`state` 可重置、`data` 需保護 |

### 四目錄語意（與 §0 同，設施須尊重）
- `config`：程式不改的設定，刪後用預設值。
- `cache`：執行時間外可任意刪，不可依賴其存在。
- `state`：階段/進度，刪後遺忘可重置。
- `data`：累積成果，不可隨意刪、需備份。

### 待拍：API 形狀（D-API）
- (1) 一組自由函式：`ac::state::path(name, dir)`、`ac::state::load/save(...)`。
- (2) 託管物件：`ac::StateStore store{program_name}; store.config()… store.save(...)`。
- 傾向：物件式 (2)——`program_name` 綁定一次，四目錄與讀寫掛在其上，較貼「託管」語意。但待你拍。

---

## 開放問題 / 後續輪次

- **D-API**：設施 API 形狀（自由函式 vs 託管物件）。
- 設施是否需要回頭和軸 3 `stateful` 連動（e.g. 託管即隱含 stateful）——目前不連動，純選用。
- 舊描述性提案的 D1/D2/D3 已因重定位作廢，不再追。
