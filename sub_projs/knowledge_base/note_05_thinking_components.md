# note_05 — 早期元件設計思考（歷史思考）

**來源**：`thinking_routing.md`、`thinking_sfc.md`、`thinking_rma.md`、`thinking_production.md`、`thinking_pending.md`
**狀態**：歷史思考（已被取代）。早期 `thinking*.md` 屬歷史思考留存，已被 `roadmap.md` 與 `core_nature/` 取代為當前事實來源；保留於此是為了無損保存各元件最初的設計與待決問題。
**一行摘要**：對五大／週邊元件的最早期構想——routing 家族（Indexer/Router/Switch/Hub）、SFC（Small Function Center）的 Layer 0~4 分層、rma（資源監控包裝、LLM Entry Manager 的鄰居）、production（生產 vs 使用維度）、pending（server/singleton/調用鏈/跨邊界調用等待設計清單）。

**現行對應內容見**：
- SFC / router / hub / indexer 原型 → [code_02_prototype_tools](code_02_prototype_tools.md)
- server / singleton / trace / call（lib 基礎設施 + llm_call）→ [code_03_prototype_lib](code_03_prototype_lib.md)
- 待決事項與決策現況 → [note_06_decisions_and_open_questions](note_06_decisions_and_open_questions.md)
- 五大元件總定位 → [doc_01_nature_overview](doc_01_nature_overview.md)、整體願景 → [note_01_vision_and_origin](note_01_vision_and_origin.md)
- 執行模型四象限（rma 對應「管資源」一格）→ [note_04_thinking_layers_model](note_04_thinking_layers_model.md)

> 演化提醒：本檔記載的多為**早期提案與待設計清單**，多數待決問題後來由 `try_implement/` 原型探索並部分收斂進規範（見 `DECISIONS.md`）。例如「Hub 完整定義待定」現由 `try_implement/tools/hub.py` 原型自定義；SFC 已改接真 `ai_core`。閱讀時以現行 code_/doc_ 為標準事實。

---

## A. Routing 家族：Indexer / Router / Switch / Hub（出自 thinking_routing.md，2026-05-21）

名詞借用自區域網路中的 hub / router / switch 概念，但實際語意有所不同。

### Indexer

掃描指定資料夾中的可執行檔，產出靜態索引（`index.md`）供查找。不做路由、不做呼叫。One-shot 程式。（與 Hub/SFC 三者區別見下方 §B 對照表。）

**Indexer 升級版（待設計）**：在基本索引之上，呼叫 AI 對被 index 的工具自動：
- 添加標籤（tags）
- 生成簡介（summary）
- 做分類（category）

產出的索引比純靜態版更豐富，方便 LLM caller 快速理解工具用途。具體設計待定。

> 演化備註：Indexer 升級版「呼叫 AI 自動加 tag/summary/category」與 `thinking_routing` 的 Hub 透鏡 metadata 擴增思路重疊，後來在 hub 原型的 context budget 逐級收斂功能裡部分體現。

### Router

**本質**：name → 某個可執行物 的 mapping，加上執行。「可執行物」可以是：
- 單檔程式的路徑（`./tools/code_senior_and_very_smart.sh`）
- JSON store 中的腳本片段（來自 SFC 的 Layer 0 資料）
- 其他任何可被執行的東西

兩種情境在概念上是同一件事——router 不在意 mapping 的目標是檔案還是資料庫內容，只負責「查到 → 執行」。

```bash
router code_senior_a --lang c    # 查表後執行對應的可執行物
```

Mapping 的來源無硬性規定：可以是人工設定檔、從 indexer 輸出動態產生、或由 AI 自動導航——標準規範待定。

**Router 升級版（待設計）**：在基本路由之上，加入：
- 使用者安全憑證檢查
- 資源消耗管理

概念上類似網路中的 router 升級到 firewall / load balancer 的方向，但具體設計尚未開始。

### Switch

**本質**：有條件邏輯的 router。Router 是單純的 mapping（一對一查表）；switch 在 mapping 之外，加入條件判斷：

> 若需求滿足某條件 → 導航到程式 A；否則 → 導航到程式 B

例如：檢查輸入語言是 C 還是 Python，分別路由到不同的 linter。Switch 借用網路 switch 的名稱，但概念差異大——這裡的 switch 更接近條件式 dispatcher，不是廣播域分割。具體標準規範待設計。

### Hub（thinking_routing 版立場）

**目前狀態**：概念尚未完整定義。已知：
- 在本系統中，hub 的定位將**脫離網路概念**，進入另一個領域
- Hub 不等同於 indexer（靜態索引）、router（name mapping）、或 switch（條件路由）
- `thinking.md` 中有「Hub 作為透鏡」的早期設計思路（擴增、過濾、彙總 metadata，見 [note_04_thinking_layers_model](note_04_thinking_layers_model.md) §C），但此概念是否仍屬於 hub 或已分散給其他工具，待重新評估

待設計：hub 的完整定義與定位。

> 演化備註：現行 Function Hub 規範仍未定案，由 `try_implement/tools/hub.py` 原型自定義（含掃描函式集、呼叫各 `--metadata`、彙整成給 LLM 的 skill 清單、context budget 逐級收斂）。見 [code_02_prototype_tools](code_02_prototype_tools.md)。

---

## B. Small Function Center（SFC）分層設計（出自 thinking_sfc.md，2026-05-21）

### Small Function 定義

Small function 是符合以下條件的函式：
- **只允許 one-shot**（multi-shot 或 persistent 無論多簡單，不算 small function——非硬性規定，但建議遵守）
- 對 input/output 做簡單處理，或執行非常簡單的邏輯、運算
- 可以套用「inline」概念——邏輯夠薄，不值得獨立成一個檔案

典型例子：`code_senior.sh` 最簡單的實作只是在 input 前面插一串 prompt 宣告「你是資深軟體工程師」，寫成程式不到兩行。

### 分層架構（由簡到繁，各層獨立可用）

#### Layer 0：純資料（無程式）

把小函式的定義存入一個 JSON 檔（array 或 object），另一個 key 或另一個檔案存 index。不需要任何額外程式——這是最簡單的集中管理形式。

```
functions.json     ← 函式定義的 array/object
index.json         ← name → 函式在 functions.json 中的位置
```

#### Layer 1a：`intake`（one-shot）

把散落的腳本片段納入 Layer 0 的 JSON 檔，並更新 index。生產端工具：將新函式「收進來」。

#### Layer 1b：Router（one-shot）

從 index 查找目標函式，mapping 到實際內容（檔案路徑或 JSON 中的腳本片段），執行之。消費端工具：使用 Layer 0 的資料。詳見上方 §A Router。

#### Layer 2：`forge`（persistent server，基礎版）

**存在理由**：Layer 1 每次呼叫都要讀檔、解析 JSON、查 index——I/O 與解析開銷在頻繁呼叫時不可忽視。`forge` 的解法：啟動時付一次讀取與編譯的代價，之後每次呼叫都是純記憶體查表與執行。

行為極簡：**啟動 → 讀取指定資料夾/檔案 → 將 tiny function 定義編譯成可呼叫函式物件 → 存入記憶體 dict → 等待 API 呼叫**。

#### Layer 3：`forge` + 管理 API

在 Layer 2 之上加入對 tiny function 的操作介面：
- 查詢：列出目前載入的函式
- 新增：動態載入並編譯新函式，加入 dict
- 刪除：從 dict 移除
- 保存：將目前記憶體中的函式定義寫回 Layer 0 的 JSON 檔

#### Layer 4：SFC Server（頂層）

在 Layer 3 之上加入：
- **資源管理**：記憶體用量、執行時間限制
- **錯誤處理**：呼叫失敗的標準化回應、retry 機制

並加上 git-style subcommand CLI 介面，讓 shell 可以直接呼叫：

```bash
sfc greet --name Alice        # 呼叫 greet
sfc word-count --input foo.txt

sfc --metadata                # SFC 自身的 metadata
sfc greet --metadata          # greet 的 subcommand-scoped metadata（§4.0）
```

> 演化備註：SFC 的 git-style subcommand dispatcher 與 subcommand-scoped metadata 後來落地為現行 `register_subcommand()` / `register_subcommand_resolver()`（動態子命令解析，名稱來自外部資料如 SFC store），由 `intercept()` 處理 `<sub> --metadata` 與 `--store` 前綴。原型 `try_implement/tools/sfc.py` 已改接真 `ai_core`。見 [code_02_prototype_tools](code_02_prototype_tools.md) 與 [doc_06_lib_contract](doc_06_lib_contract.md)。

### Dispatcher 機制（Layer 2）

SFC 內部維護 global dict：

```
{ "function_name": <function_object>, ... }
```

subcommand 名稱作為字串 key 查表，直接呼叫對應的 function object。

### Tiny Function 定義格式

格式應非常靈活，常見形式：
- **Shell pipe**（最常見）：一串 shell 指令，支援 pipe 串接
- **Python**：小段 Python 邏輯
- **Lua**：小段 Lua 邏輯

具體格式（設定檔結構）待設計。

#### In-process 的程度（Layer 2）

| 函式類型 | 執行方式 |
|---|---|
| Python / Lua | 真正 in-process，SFC 直接在自身 runtime 執行，無 subprocess |
| Shell pipe | SFC 內部開 shell subprocess——subprocess 存在，但由 SFC 管理，dispatch 決策在 SFC 內完成 |

### 動態 API（待設計）

SFC server 計劃提供以下 API：
- **新增**：執行期間動態添加新的 tiny function
- **持久化**：將目前記憶體中的函式定義存回 Layer 0 的 JSON 檔

### Hub / SFC / Indexer / Router 的關係

| 工具 | Layer | 本質 |
|---|---|---|
| Indexer | — | 掃描可執行檔，產出靜態索引 |
| `intake` | 1a | 把腳本片段納入 JSON store，更新 index |
| Router | 1b | 從 JSON store 的 index 查找，mapping 後執行 |
| SFC server | 2 | Router 的 persistent 升級版，in-process 執行 |

### SFC 待設計清單

- Tiny function 設定檔（Layer 0）的具體格式
- 呼叫介面細節（參數傳遞方式）
- 動態 API 的具體介面
- SFC 與 hub 的協作：hub 如何得知 SFC 旗下有哪些 subcommand

---

## C. rma：資源監控包裝程式（出自 thinking_rma.md，2026-05-21）

> rma 是 LLM Entry Manager（資源管理單例）的鄰居／前身思路——同樣處理「受限資源管控」，但 rma 聚焦在 shell 層的記憶體/時間/CPU，而非 LLM token rate。

### 動機

Shell 層面的記憶體與執行時間管理需要一個統一工具。Linux 已有相關工具：

| 需求 | 現有 Linux 工具 |
|---|---|
| 執行時間限制 | `timeout N cmd` |
| 記憶體 / CPU 時間 / 檔案限制 | `ulimit`, `prlimit` |
| CPU 優先度 | `nice`, `renice` |
| 測量用量（事後） | `/usr/bin/time -v`, `/proc/<pid>/status` |
| 完整資源隔離 | `cgroups v2`, `systemd-run --scope` |

**設計原則**：能利用的就利用，除非需求太特殊，作業系統無法快速搞定，否則不重複造輪子（呼應 No wheel-remake）。rma 要做的，要麼是**統一多工具的呼叫介面**，要麼是**提供 Linux 工具沒有的特殊服務**；重複的部分要避免。

### 介面設計

```bash
rma --exec code_senior.sh --args lang c   # 包裝執行（rma 管理資源）
code_senior.sh --lang c                   # 普通執行（無資源管理）
```

實作語言：**C**，直接使用 `setrlimit()`、`getrusage()`、`fork()`、`waitpid()`，無 runtime 依賴，開銷低。

### rma 相對 Linux 工具的獨特價值

1. **統一介面**：`timeout`、`prlimit`、`nice` 語法各不同；rma 用單一介面同時管多個指標
2. **Alert callback**：超出閾值時呼叫指定的 `.sh`（Linux 工具只有 kill，沒有 callback）
3. **結構化 log**：用量統計寫入指定 JSON 檔，方便 hub / AI caller 事後讀取
4. **軟警告 + 延遲強殺**：先 SIGTERM + grace period，再 SIGKILL；`timeout` 沒有 grace period 機制
5. **`--json-errors` 整合**：違規時的 stderr 遵守 ai_core 錯誤協議（`{"type": "MemoryExceeded", ...}`）

監控維度包括：記憶體用量（RSS）、執行時間（wall time / CPU time）、CPU 核心占用。

### 與執行模型表格的關係

rma 對應 [note_04_thinking_layers_model](note_04_thinking_layers_model.md) §D 執行模型表格中「管資源」的一格：

| | one-shot | persistent |
|---|---|---|
| **不管資源** | pipe-and-script | — |
| **管資源** | **one-shot 程式**（rma 包裝這一層） | 持續性程式 |

rma 是讓 pipe-and-script 升格為「受資源管控的 one-shot」的標準工具。

> 演化備註：rma 的資源管控概念演化成現行 `resources` 軸。實作上 rma（C 程式）尚未在現行 repo 中落地；資源管理思路集中體現於 LLM Entry Manager 原型 `try_implement/tools/llm_entry_manager.py`（consume rate：token/金錢/本地 GPU）。

---

## D. 生產 vs 使用維度（出自 thinking_production.md，2026-05-20）

### 兩個維度：使用 vs 生產

前面所有的討論（index / metadata / hub / outside_progs / inside_procs）都是在處理**使用管理**——如何找到、描述、呼叫已存在的程式或函數，且預設是人類手寫的邏輯、one-shot 的情境。

這裡開始思考另一個同樣基礎的維度：**生產管理**——如何產生程式或函數。生產者可以是：
- 人類手寫
- 程式生成的程式（meta-programming）
- AI 生成的程式（LLM codegen）

### 命令行世界的生產：pipeline 模式

CLI 單檔程式的生產方式很自然，用 pipe 串接：

```bash
code_seniors.sh --lang c \
  | prompt_appen.sh "generate a for loop" \
  | text_inserter.sh --file "main.c" --line 43
```

這條 pipeline 的語意是：「產生一段 C 的 for loop，插入 main.c 第 43 行」。輸出物是**檔案的一部分**，生產過程是一次性的 pipe。

### 待思考的問題

**1. 產出物很小的情況**：當產出物不是一整個程式，而是一個函數、一個片段、甚至一行程式碼時，pipeline 的開銷（行程啟動、stdin/stdout 傳遞）可能遠大於產出物本身。這時：
- 是否應該直接在程式內部呼叫 LLM，而不走 shell pipe？
- 產出的小片段如何被納入 index / metadata / hub 的管理框架？

**2. 程式內函數的生產**：對於程式內部動態生成的函數（參見 [note_04](note_04_thinking_layers_model.md) §E 邊緣案例）：
- 生成後要如何自動登錄進 `FunctionManager` / registry？
- 生成的函數沒有靜態原始碼位置，index 如何處理？

**3. 生產與使用的銜接**：生產出來的東西，最終還是要被「使用」。生產管理要如何與使用管理的三個理念（index / metadata / hub）銜接？
- 生產完成後，自動補上 metadata？
- 自動登錄進 hub？

> 演化備註：「生產 vs 使用」維度演化成現行 roadmap 的「資產工廠（聰明模型生資產）vs 消費者（笨模型用資產）」框架，以及第一目標問題「程式碼輔助助手」的上下層分工（上層讀框架生資產、下層在標記位置填碼）。「AI 生成函式後如何自動補狀態宣告」在 thinking_state 也被列為尚未處理；對應現行刻意暫緩至 v0 的 B 系列與「固化引擎手動 vs 自動」最硬未決題。見 [note_06](note_06_decisions_and_open_questions.md)。

---

## E. 待設計主題清單（出自 thinking_pending.md，2026-05-21）

> 這是早期的彙總待辦清單。多數項目後來由原型探索或規範扶正處理。

### §1 路由相關工具設計（Indexer / Router / Switch / Hub）

詳細設計記錄見上方 §A（原 `thinking_routing.md`）。

已確認拆分：
- **Indexer**：掃描 → 靜態索引，已確認
- **Router**：name mapping + subprocess dispatch，mapping 來源無硬性規定（設定檔、AI 導航等），標準規範待定
- **Switch**：有條件邏輯的 router，標準規範待定
- **Router 升級版**：加入安全憑證檢查 / 資源消耗管理，待設計

待設計：Router mapping 標準規範、Switch 標準規範、Router 升級版具體設計、**Hub 的完整定義**（已確認將脫離網路概念，進入另一個領域，尚未想好）。

### §2 Small Function Center（SFC）設計

詳細設計記錄見上方 §B（原 `thinking_sfc.md`）。

待設計：Tiny function 設定檔的具體格式、呼叫介面細節（參數傳遞方式）、動態 API（新增 / 持久化 tiny function）、SFC 與 hub 的協作（hub 如何得知 SFC 旗下有哪些 subcommand）。

### §3 持久性程式設計（persistent → server）

現有記錄：[note_04](note_04_thinking_layers_model.md) §D（執行模型分類，原 `thinking_layers.md`）。

已確認立場：
- **持久性程式基本上建議設計成 server**（在 one-shot 與 multi-shot 之外的第三條路）
- `thinking_layers.md` 裡 JSON-RPC over stdin/stdout 的選項需要重新評估或更新

待設計：
- Server 的標準 lifecycle（啟動、就緒、處理請求、關閉）
- 對外介面規範（HTTP API 的基本 endpoint 慣例）
- 與 `--metadata` 協定的整合（server 啟用提示已有 §4.8，但 lifecycle 本身還沒有）
- Persistent 程式的 snapshot/checkpoint 機制（`thinking_layers.md` 提到「本質是 mini git」，尚未設計）

> 演化備註：persistent/server lifecycle 思路體現於 lib 基礎設施與 LLM Entry Manager 原型。見 [code_03_prototype_lib](code_03_prototype_lib.md)。

### §4 Singleton 資源設計

現有記錄：`CLAUDE.md`（LLM Entry Manager 是標準案例）、（當時的）`docs/architectures/03_entry_manager.md`。

已知：
- Singleton 的觸發條件：資源有限，且請求者是多個不認識彼此的 caller
- 標準模式：queue + consume rate 管理
- LLM Entry Manager 是這個模式的具體實例

待設計：
- 通用 singleton 模式的抽象：什麼是「singleton 資源」的完整定義
- Queue 的標準介面（enqueue、dequeue、cancel）
- Consume rate 管理的標準介面（token、金錢、算力等不同維度）
- 多個 singleton 資源之間的協調（例如 LLM entry A 與 entry B 共用 GPU）

> 演化備註：singleton 資源模式由 LLM Entry Manager 原型（`try_implement/tools/llm_entry_manager.py`，佇列模式 + consume rate 管理）具體化。LLM 是單例資源（一次一請求）。見 [code_03_prototype_lib](code_03_prototype_lib.md)。

### §5 調用鏈設計

現有記錄：[note_04](note_04_thinking_layers_model.md) §D（輕量追蹤方案，原 `thinking_layers.md`）、（當時的）`docs/architectures/02_protocol.md`（§5 錯誤處理慣例）。

**定義**：**調用鏈**在 shell 世界中指：process 與其子 process，或 pipeline 的前後節點。**程式內部的函數調用鏈**不在標準規範範疇內，由程式自行處理。

**已確認立場：多調用鏈並發**——標準規範不考慮多個調用鏈同時進行的問題。若使用者有相關需求，需自行處理相關風險。

| 問題 | 標準規範立場 |
|---|---|
| 呼叫順序 | 不管——由使用者自行決定與協調 |
| 資源搶占 | 建議將需要謹慎存取的資源交由某個持續性 server 管理；使用 singleton 也是推薦選擇 |

**追蹤與可操作性（待設計）**：已知輕量方案——每個工具在 stderr 輸出結構化 JSON log，成本接近零，由最外層 caller 決定是否收集。待設計：
- 調用鏈 ID 的傳遞機制（如何讓整條鏈共享同一個 trace ID）
- 收集工具：誰來彙整 stderr 的結構化 log、輸出成可讀的調用樹
- 可操作性：能否根據調用鏈紀錄重跑某一段、跳過某個節點、注入替代輸出

**錯誤處理（待設計）**：已知——stdout 正常結果、stderr 錯誤、exit code 非 0 表示失敗；`--json-errors` 旗標讓 stderr 輸出結構化 JSON 供程式 caller 讀取。待設計：
- 調用鏈中途失敗時的傳播策略（fail-fast vs 繼續執行並標記）
- Retry 策略的標準化（`retriable` 欄位已有，但 retry 的實際行為由誰負責？）
- 部分失敗的彙報格式（整條鏈結束後如何彙整哪些節點成功、哪些失敗）

> 演化備註：trace 概念與 chain 工具體現於現行原型 `try_implement/tools/chain.py` 與 lib 基礎設施。見 [code_02_prototype_tools](code_02_prototype_tools.md) / [code_03_prototype_lib](code_03_prototype_lib.md)。

### §6 跨邊界調用的統一設計（含分散式狀態）

現有記錄：[note_04](note_04_thinking_layers_model.md) §E（`outside_progs` 統一呼叫介面，原 `thinking_oop.md`）。

**已確認立場**：**API 調用（HTTP）、shell 調用（subprocess）、程式內部函數調用，本質上是同一件事**——函式呼叫跨越不同的邊界：

| 邊界 | 呼叫形式 |
|---|---|
| 程式內部 | 直接函式呼叫 |
| 跨 process（shell） | subprocess + stdin/stdout |
| 跨網路（API） | HTTP request / response |

三者概念相同，只是傳遞機制不同。`outside_progs` 的統一呼叫介面建立在這個等價性之上。

**待設計**：考慮**分散式系統的狀態問題**——當調用跨越網路邊界時，狀態的一致性、失敗恢復、冪等性等問題與本地調用有本質差異，需要專門設計。

> 演化備註：冪等性問題後來收斂成現行第七軸 `guarantee`（none<idempotent<transactional）。call 統一介面思路體現於 lib 的 llm_call 與基礎設施。見 [code_03_prototype_lib](code_03_prototype_lib.md)。
