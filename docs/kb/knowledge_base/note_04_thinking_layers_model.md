# note_04 — 早期分層架構與本質思考（歷史思考）

**來源**：`thinking.md`、`thinking_layers.md`、`thinking_oop.md`、`thinking_state.md`、`thinking_core_nature.md`
**狀態**：歷史思考（已被取代）。早期 `thinking*.md` 屬歷史思考留存，已被 `roadmap.md` 與 `core_nature/` 取代為當前事實來源；保留於此是為了無損保存獨特概念與演化脈絡。
**一行摘要**：ai_core 最早期的整體構想——以「執行模型」而非「技術層」定義結構、metadata/index/hub 三大貫穿理念、CLI 與 OOP 的對稱統一、multi-shot 狀態模型，以及本質定位的切分起點。

**現行對應內容見**：
- 分層／本質 → [doc_01_nature_overview](doc_01_nature_overview.md)
- 執行狀態／執行形式 → [doc_03_execution_forms](doc_03_execution_forms.md)（multi-shot 狀態、`.state`/`.data` 等）
- 整體願景／初心 → [note_01_vision_and_origin](note_01_vision_and_origin.md)
- 九軸／metadata 軸定義現況 → [doc_05_axes_metadata](doc_05_axes_metadata.md)
- register/intercept library 契約現況 → [doc_06_lib_contract](doc_06_lib_contract.md)
- CLI 契約現況 → [doc_07_cli](doc_07_cli.md)

> 注意演化落差：本檔記載的許多 metadata 欄位（`execution_model`、`reversible`、`undo_method`、`retry_safe`、`memory_hints`、`complexity`、`semantic_coupling`、`is_multishot`、`multi_shot_dirs`）是**早期提案**，與現行已定案的九軸（`entries`/`lifecycle`/`state`/`state_dirs`/`resources`/`interruptible`/`guarantee`/`dry_run`/`nondeterministic`）並非一一對應。base class（OOP `Function(ABC)`）的設計方向後來被 `register()/intercept()` 純宣告模型取代。閱讀時請以九軸與 register/intercept 為現行標準事實。

---

## A. 專案定位：為 Heuristic Learning 提供基礎設施（出自 thinking.md，2026-05-20）

**ai_core 的用途是為 Heuristic Learning 提供基礎設施。**

Heuristic Learning（HL）是一種替代梯度更新的迭代範式：coding agent 直接編輯策略程式碼，環境回饋驅動下一輪改進，舊能力固化為測試與版本差異，而非消失在模型權重裡。

參考：[Learning Beyond Gradients](https://trinkle23897.github.io/learning-beyond-gradients/#zh)

ai_core 各元件對應的 HL 角色：

| ai_core 元件 | HL 角色 |
|---|---|
| `entry_manager` + `client` | LLM 監督者的呼叫介面 |
| `author` | agent 產生 / 修改策略函式 |
| `hub` + `funcs/` | 策略函式庫（Heuristic System） |
| `author` dry-run + retry | 環境回饋驅動的改進迴圈 |
| `--metadata` 介面 | agent 理解工具的唯一入口 |

> 這個「HL 基礎設施」定位是早期願景的一種表述，與現行 `roadmap.md` 的「輔助小笨模型 + 資產工廠 + 拒絕為預設」框架是同源但措辭不同的演化前身。

---

## B. Metadata 設計補充（出自 thinking.md，2026-05-20）

> 朝 coding agent 一側看，能接受多少耦合複雜度，取決於模型能力、上下文長度、memory 質量、工具質量、整體迭代速度。

**metadata 的職責是把認知負擔從 agent 身上搬走。** 這意味著 metadata 不只是描述「函式是什麼」，還要描述「agent 使用這個函式時需要多少認知資源」。

當時 `MetadataView` 的缺口：

| 缺失欄位 | 對應維度 | 說明 |
|---|---|---|
| `complexity` / `cognitive_load` | 模型能力 | 這個函式有多難用對？影響 agent 是否應拆解或迴避 |
| `memory_hints` | memory 質量 | 成功執行後哪些輸出值得跨輪次記住——**最優先補的欄位** |
| `idempotent` / `retry_safe` | 迭代速度 | 重跑有副作用嗎？agent 判斷是否重試時不能靠猜 |
| `semantic_coupling` | 工具質量 | 使用模式上常與哪些函式搭配——與 `dependencies`（技術依賴）不同 |

`memory_hints` 最關鍵：HL 的價值建立在跨輪次積累上，若函式不宣告哪些輸出值得記，agent 只能靠猜，memory 質量就會不穩定。

> 演化備註：`idempotent`/`retry_safe` 的關注點後來收斂成現行第七軸 `guarantee`（none<idempotent<transactional）。`memory_hints`/`complexity`/`semantic_coupling` 等「語意/用途欄位」在 `DECISIONS.md` 被歸入刻意暫緩至 v0 的 B1 系列。

---

## C. Index 與 Metadata 的可變性，以及 Hub 作為透鏡（出自 thinking.md，2026-05-21）

### 核心洞見：index 與 metadata 都不是靜態的

**Index 的兩層語意：**

| 層次 | 說明 | 穩定性 |
|---|---|---|
| 規範索引（canonical） | 絕對路徑 / module 完整路徑，指向「這個東西本身」 | 穩定，不隨環境改變 |
| 參照（reference） | 相對路徑 / hub-local name，指向「在某個上下文中怎麼找到它」 | 隨工作目錄、所在 hub 等上下文變動 |

相對路徑隨工作目錄改變，這不是缺陷——reference 是 canonical index 在特定上下文中的投影形式。

**Metadata 的可變性：**

Metadata 可以依 **subcommand 結構**改變，類比 `git --help` vs `git remote --help`：

- `xxx.sh --metadata` → 全域 metadata
- `xxx.sh subcommand --metadata` → 該 subcommand 的 metadata

**輸入不影響 metadata。** 無論 stdin、pipe 上游、還是 flag 帶入的資料，都不改變 metadata 的輸出。以 `-` 或 `--` 開頭的 flag 不得與 `--metadata` 混用（混用應報錯）。

Metadata 的可變性只有一個軸：**靜態的命令層次（subcommand）**，而非動態的執行期資料。

> 演化備註：subcommand-scoped metadata 後來落地為現行 `register_subcommand()` / `register_subcommand_resolver()`，並由 `intercept()` 處理 `<sub> --metadata` 與 `--store` 前綴。

### Hub 是透鏡（lens）

Hub 是 index 的聚集，但對 hub 的**呼叫者**而言，hub 重新定義了 index 與 metadata 的形貌：

**Index 的重定義（命名空間化）**：
```
原始 canonical index：  ./tools/professor.sh
透過 hub 看到的 index：  prompt-hub/professor
```
Hub 提供一個新的命名空間層，把分散的路徑收攏為統一的邏輯名稱。

**Metadata 的重定義（透鏡式轉換）**：Hub 可以對 metadata 做：
- **擴增**：加上 hub 自己的分類、tag、排序權重
- **過濾**：只暴露 `summary`，省略 `description`，節省 caller 的 context 用量
- **彙總**：把語意相近的函式群組成一份聯合描述

> 透鏡不改變物體本身，只改變觀察者看到的形象。Hub 的本質是**聚合 + 轉換**，不只是被動的目錄列表。

**推論：Hub 可以被組合。** Meta-hub 把多個 hub 的 index 再聚合，形成更高層的透鏡。每一層透鏡各自做轉換，外層 caller 只看到最終的聚合結果。

### thinking.md 歷史紀錄附註

2026-05-18 之前的所有設計點已遷移至（當時的）`docs/architectures/`，對應表見舊版 `thinking.md`（git history）。

---

## D. 用「用途」而非「層」定義結構（出自 thinking_layers.md，2026-05-20）

### 核心框架

系統的結構由**執行模型（execution model）**決定，不是由技術層次決定。沿著兩個軸展開：

|  | one-shot（請求完就結束） | persistent（持續存在） |
|---|---|---|
| **不管資源** | shell pipe / script | — |
| **管資源** | one-shot 程式 | 持續性程式 |
| **管併發** | — | server |

Hub 是橫切的——它是這些東西的目錄與彙整，不屬於哪一格。

> 演化備註：這個「執行模型四象限」是現行 `lifecycle`（one_shot/persistent）軸與 `resources` 軸的思想前身；server 觸發條件後來在 `thinking_pending` §3 被確認為「持久性程式基本上建議設計成 server」。

### 各執行模型說明

**pipe-and-script**：外部工具、輕量、快速、命令行操作。直接用 shell pipe 串接，頂多加 shell script 做膠水黏合。

```bash
professor.sh --type "senior c coder" \
  | modify_c_code_segment.sh --file main.c --line 44-55 --prompt "no pointer" \
  | code_linter.sh --lang c \
  > new_main.c
```

不在乎執行時間與記憶體，隨手可用，用什麼語言實作都可以。

**one-shot**：需要記憶體或執行時間管理的單次程式。請求進來、處理完、結束。可以用任何語言寫，但此時 shell script 已不夠用。仍可被 pipe 串接或直接呼叫。

**persistent**：需要多輪複雜狀態保存與管控的長期程式。特徵：
- 用 Ctrl-C 取消
- 把狀態存到外部檔案
- 在 stdout 持續輸出狀態
- 本質上是單一使用者的長期存在行程

**對外介面選項**：one-shot 工具要使用 persistent 程式，有兩條路：
- **HTTP API**（往 server 方向走，引入更多複雜度）
- **JSON-RPC over stdin/stdout**（LSP 模式）：輕量、不需要 port、不需要 HTTP stack，一個 shell 直接對接，同時保留「不是 server」的身份——這條路更適合 persistent 程式的定位

> 待重評註記：`thinking_pending` §3 指出，由於「持久性程式基本上建議設計成 server」，這裡的 JSON-RPC over stdin/stdout 選項需要重新評估或更新。見 [note_05_thinking_components](note_05_thinking_components.md) §持久性程式。

**server**：觸發條件比「用了 HTTP API」更本質——**資源是有限的，且請求者是多個不認識彼此的 caller**。LLM entry 是標準案例：token rate limit 是共享資源，多個 caller 不知道彼此，所以需要 queue 和 server 集中管理。用來處理高併發、高流量、佇列任務、管控受限資源。

### Hub 的定位（thinking_layers 版）

單一工具在檔案系統中的路徑本身就是一種索引（可尋址）。Hub 的功能不是「建立索引」，而是把分散的索引**聚集**起來——例如 `prompt_hub` 把所有 prompt 片段工具集中成一份清單。

當外部工具或 shell script 很多時，hub 做一個整合彙整的簡單程式。實作語言隨便，重點是理念：掃描可用工具、收集 metadata、彙整成清單。

### 邊緣議題：「上一步」的語意隨執行模型不同

「上一步」問的是：**這個程式的副作用是不是可逆的？用什麼機制可逆？**

| 執行模型 | 上一步的實現方式 |
|---|---|
| pipe-and-script | 只能靠 checkpoint 檔案，pipe 本身不可逆 |
| one-shot | 重跑就是上一步，前提是 idempotent（`retry_safe` metadata 欄位） |
| persistent | 需要 snapshot/checkpoint 機制，本質是 mini git |
| server | cancel task + undo task 副作用（undo 目前尚未設計） |

這個問題可以加進 metadata：`reversible: bool` + `undo_method`（描述如何撤銷）。

### 邊緣議題：調用鏈管理（輕量做法）

Ledger server 已移除（太後期），但輕量的調用鏈追蹤可以很早就有用：**每個工具在 stderr 印一行結構化紀錄**。

```json
{"caller": "modify_c_code_segment.sh", "tool": "professor.sh", "status": "ok", "ms": 320}
```

成本接近零，由最外層的呼叫者決定要不要收集。讓 debug 從「不知道哪步壞了」變成「能看到整條鏈」。

> 此調用鏈思路在 `thinking_pending` §5 有後續展開，見 [note_05_thinking_components](note_05_thinking_components.md)。

### Metadata 新增欄位小結（thinking_layers 提案）

基於以上討論，`MetadataView` 應補充：

| 欄位 | 型別 | 說明 |
|---|---|---|
| `execution_model` | `str` | `pipe-and-script` / `one-shot` / `persistent` / `server` |
| `reversible` | `bool` | 副作用是否可撤銷 |
| `undo_method` | `str` | 如何撤銷（`rerun`、`snapshot`、`api-cancel`、`none`…） |
| `retry_safe` | `bool` | 重跑是否安全（idempotent） |
| `memory_hints` | `list[str]` | 成功執行後哪些輸出值得跨輪次記住 |
| `complexity` | `str` | `low` / `medium` / `high`，agent 判斷認知負擔用 |
| `semantic_coupling` | `list[str]` | 使用模式上常與哪些函式搭配 |

---

## E. OOP 對應與 CLI/OOP 統一（出自 thinking_oop.md，2026-05-20）

### 貫穿始終的三個理念：metadata / index / hub

**metadata**（程式對自身的描述）、**index**（程式/函數的位址）、**hub**（聚集索引）是貫穿整個系統的三個核心理念。理念本身不變，但在不同邊界上以不同的形式出現：

| 邊界 | metadata 的形式 | index 的形式 | hub 的形式 |
|---|---|---|---|
| 命令行（單檔程式） | `--metadata` flag → JSON | OS 檔案系統路徑（`./code_senior.sh`） | hub scanner 掃描可執行檔 |
| 程式內部（函數） | `ip.register()` 的描述參數 | 原始碼位置（`file.py:44`）或 runtime registry | `FunctionManager` / registry |
| Python library（class） | base class 的類別屬性 + `metadata()` | module 路徑（`mymodule.Professor`） | hub scanner 掃描 subclass |

形式不同，理念相同。設計新機制時，先問：「這裡的 index 長什麼樣？metadata 長什麼樣？hub 長什麼樣？」

### metadata 與 hub 的本質

- **metadata** 的本質：程式對自身的描述
- **hub** 的本質：對程式的**聚集**索引

#### 索引的三個層次

| 層次 | 索引誰 | 天然索引 | 聚集器 |
|---|---|---|---|
| 跨程式 | 獨立可執行檔 | 作業系統檔案系統路徑（`/bin/program`） | hub |
| 程式內部 | 程式內的函數 | 原始碼位置（`file.py:44`） | `FunctionManager` / `function_indexes` 全局 class |

兩層的結構完全對稱：環境本身都已提供天然索引，聚集器的工作只是把這些分散的天然索引**收攏成一份清單**。

**邊緣案例**：執行期間動態生成的函數沒有靜態行號可用作天然索引，只能由程式自行維護額外的索引結構（dict、registry 等）。此情況（當時）暫不設計。

Hub 做的是**跨程式的聚集**，`FunctionManager` 做的是**程式內的聚集**，兩者是同一概念在不同邊界上的顯示形象。`--metadata` flag 與 hub scanner 是這兩個概念在**命令行世界**的顯示形象。在 **Python library 世界**，同樣的概念自然對應到 OOP：**繼承於提供這兩個功能的 base class**。

### CLI 與 OOP 的統一

CLI 世界的 `--metadata` 是 base class metadata 的**跨行程序列化格式**。Base class 本身就是 metadata；`--metadata` 只是把它印成 JSON 讓另一個行程能讀到。兩者是同一個概念在不同邊界上的顯示形象。

```
Python class  ──(同一個行程)──→  MetadataView 物件
CLI 執行檔    ──(行程邊界)────→  --metadata → JSON → MetadataView 物件
```

Hub 不需要知道來源是哪種，終點都是 `MetadataView`。

### Base Class 設計（早期提案）

```python
class Function(ABC):
    # 類別層級宣告 metadata（子類別 override）
    name: str = ""
    description: str = ""
    execution_model: str = "one-shot"   # pipe-and-script / one-shot / persistent / server
    retry_safe: bool = True
    reversible: bool = False
    undo_method: str = "none"
    memory_hints: list[str] = []
    complexity: str = "low"
    semantic_coupling: list[str] = []
    # ... 其他 MetadataView 欄位

    @classmethod
    def metadata(cls) -> MetadataView:
        """從類別屬性組出標準 MetadataView。"""
        ...

    @abstractmethod
    def run(self, input: str) -> str:
        """子類別實作實際邏輯。"""
        ...
```

**設計原則**：base class 只負責**宣告介面**（metadata 欄位 + 抽象 `run()`）。Auto CLI 生成、hub 自動註冊等功能作為獨立的 opt-in 工具，不內建於 base class——否則違反 KISS，base class 會變成 framework。

> 演化備註：這個 ABC base class 路線**未被採用**。現行定案是 `src/ai_core/_core.py` 的純宣告函式 `register()/register_subcommand()/register_subcommand_resolver()` + 顯式 `intercept()`，刻意避開繼承與 framework 化。見 [doc_06_lib_contract](doc_06_lib_contract.md)。

### 推論一：Auto CLI 生成

有了 base class，CLI wrapper 是純樣板程式碼，可以自動產生。`--input`、`--output`、`--metadata`、`--json-errors` 全部從 base class 來。**寫 Python class 就同時得到 CLI 介面，不用手寫兩份。**

### 推論二：Hub 統一掃描兩個世界

| 來源 | 掃描方式 | 終點 |
|---|---|---|
| 可執行檔 | 呼叫 `--metadata`（CLI 協定） | `MetadataView` |
| Python module | import 後找繼承 `Function` 的 subclass | `MetadataView` |

### 推論三：`execution_model` 驅動不同 lifecycle

Base class 可依宣告的 `execution_model` 提供不同的方法簽名（opt-in subclass）：

| execution_model | 方法簽名 |
|---|---|
| `one-shot` | `run(input) -> output` |
| `persistent` | `start()` / `on_message()` / `stop()` |
| `server` | 產生 FastAPI app |

### 統一呼叫介面（Unified Call Interface）

索引解決「找到在哪」，呼叫介面解決「如何使用」。兩者合起來才是完整的抽象。

#### outside_progs：從外部呼叫單檔程式或 module

目標：不論目標是 shell script、Python module 內的函數、還是 server，呼叫端一律用相同介面：

```python
import outside_progs as op

ret = op.call("../code_senior.sh", {"lang": "c"})
ret = op.call("mymodule.Professor", {"type": "senior c coder"})
```

第一個參數是**天然索引**（路徑或 module 位址），第二個參數是輸入。library 內部依據 `execution_model`（從 metadata 取得）自動分派：

| execution_model | 內部實作 |
|---|---|
| `pipe-and-script` | subprocess 呼叫，stdin/stdout 傳遞 |
| `one-shot` | subprocess 或直接 import + 呼叫 `run()` |
| `persistent` | JSON-RPC over stdin/stdout |
| `server` | HTTP request |

呼叫端不需要知道底層是哪種，metadata 讀一次就夠。

> 跨邊界調用等價性在 `thinking_pending` §6 有後續確認立場，見 [note_05_thinking_components](note_05_thinking_components.md)。

#### inside_procs：程式內函數對外暴露

程式內的函數若要被外部的單檔程式或 shell 呼叫，程式必須主動暴露這些函數。`inside_procs` 做兩件事：

1. **建立內部 registry**（即前述 `FunctionManager`）：`ip.register()` 把函數登記進去
2. **自動生成 CLI dispatch**：讓外部可以用命令列指名呼叫特定函數

```python
import inside_procs as ip

ip.register(func_code_senior, "code_senior", {"lang": "c"})
```

外部單檔程式透過命令列呼叫（假設程式名為 `seniors`）：

```bash
seniors --ip "code_senior" --lang "c"
```

`--ip` 路由到 registry 中對應的函數，其餘參數作為輸入傳入。`inside_procs` 與 `outside_progs` 是一對：一個負責「把內部暴露出去」，一個負責「從外部統一呼叫進來」。

> 演化備註：`ip.register()` 是現行 `register()` API 的命名前身，但語意已改變——現行 `register()` 是純宣告、零副作用（不讀 argv、不攔截、不 exit），而早期 `ip.register()` 同時負責 registry 登記 + CLI dispatch 生成。

### 與 `protocol/` 的關係

Base class 自然放在 `src/ai_core/protocol/` 下——`protocol/` 已定義為 function 作者與 manager 作者的共用 helper 出口。這個 base class 就是 Python 世界裡的「協定」本身。

> 演化備註：現行核心程式碼集中在 `src/ai_core/_core.py`，並非 `protocol/` 子目錄。

---

## F. Multi-shot 狀態模型（出自 thinking_state.md，2026-05-21）

### Multi-shot 本質定義

**Multi-shot** 是一個抽象描述：**上次執行的結果會影響到下一次執行的程式**。當前（早期）的標準實現是針對 terminal 狀態（CLI / shell 世界）定義的。

### 標準實現規範（Terminal 版）

#### 狀態檔位置

程式 `XXX` 執行時，在其**工作目錄**下存取以下路徑：

```
.config/<XXX>/      ← 或 .config/<XXX>.json（單檔形式）
.cache/<XXX>/       ← 或 .cache/<XXX>.json
.state/<XXX>/       ← 或 .state/<XXX>.json
```

四個標準目錄各自語意：

| 目錄 | 語意 |
|---|---|
| `.config` | 程式本身不會修改的設定（由人類或外部工具寫入） |
| `.cache` | 可在程式執行時間之外被任意刪除，程式不依賴其存在 |
| `.state` | 程式目前所在的 stage——執行進程、跨輪次的階段位置 |
| `.data` | 程式累積建造出來的成果性資料；重要、不可隨意刪除 |

`.state` 與 `.data` 的分野：`.state` 清掉只是忘記做到哪（可重置），`.data` 清掉是真的失去成果（需備份）。

沒有硬性規定四者都必須存在；標準實現要求實作者**按語意選用**。

#### 檔案格式規則

- 每個位置可以是**資料夾**或**單檔**
- **單檔**（如 `.config/<XXX>.json`）：**必須是 `.json`，且內容為 JSON 物件**
- **資料夾內的檔案**（如 `.config/<XXX>/YYY.json`）：格式無限制

#### 版本管理

標準實現規範**不包含版本管理**（刻意省略，保持簡單）。

> 演化備註：`.state` / `.data` / `.config` / `.cache` 的概念演化成現行 `state`（stateless/stateful_external）軸與 `state_dirs` 軸。詳見 [doc_05_axes_metadata](doc_05_axes_metadata.md)。

### 本質深究：從 Terminal 到程式內函數

| 邊界 | multi-shot 的實現形式 |
|---|---|
| Terminal（CLI） | 工作目錄下的 `.config`、`.cache`、`.state` 檔案 |
| 程式內函數 | 由程式內部的 global manager 管理等效的 in-memory 結構 |

程式函數被呼叫一次 = terminal 下單一程式被呼叫一次。`.config`、`.cache`、`.state` 的概念在程式內部同樣存在，只是形式從檔案變成記憶體中的結構。

Library 提供統一介面，讓使用者不必手動操作檔案或 manager——（原檔此處討論尚未完成，訊息中斷）。

### 並發執行

核心規範不處理。標準實現建議：若 `XXX` 預期多實例並發，寫入 `.state/<XXX>` 時標註自身 PID。讀寫鎖由 OS 處理，具體標註格式與看見他人標註時的行為，由 `XXX` 撰寫者自行決定。

### Metadata 宣告慣例（早期提案）

建議在 metadata 中加入：

```json
{
  "is_multishot": true,
  "multi_shot_dirs": ["state", "data"]
}
```

`is_multishot: true` 是入口——值為 true 時，caller / hub 才進一步讀 `multi_shot_dirs`。`multi_shot_dirs` 列出此程式實際使用的目錄，省略則不宣告。

> 演化備註：`is_multishot` + `multi_shot_dirs` 是現行 `state` 軸 + `state_dirs` 軸的提案前身。

### 與其他 thinking 的關係（thinking_state 自述）

- `thinking_layers.md`：persistent 執行模型的 snapshot/checkpoint，建立在這裡的 `.state` 機制之上
- `thinking_production.md`：AI 生成函式後如何自動補上狀態宣告——尚未處理（見 [note_05_thinking_components](note_05_thinking_components.md) §production）

---

## G. core_nature 本質的切分起點（出自 thinking_core_nature.md，2026-05-26）

此檔在歷史上已被切分，內容移至 `core_nature/` 子資料夾。其當時的子檔案索引（記錄 core_nature 規範家族的最初分檔結構）：

| 檔案 | 內容 |
|---|---|
| `core_nature/overview.md` | 為什麼從 terminal 出發；本質是什麼（待填）→ 現行見 [doc_01_nature_overview](doc_01_nature_overview.md) |
| `core_nature/terminal_model.md` | §1 POSIX shell 執行模型、CLI 慣例、subcommand → 現行見 [doc_02_terminal_model](doc_02_terminal_model.md) |
| `core_nature/cli_spec.md` | §2 本規範的 CLI 標準（argparse + Lisp 概念模型）→ 現行見 [doc_07_cli](doc_07_cli.md) |
| `core_nature/data_format.md` | §3 JSON 作為通用資料格式 |
| `core_nature/execution_forms.md` | §0 分類軸總覽；§4 函式執行形式毛料清單（18 種形式）→ 現行見 [doc_03_execution_forms](doc_03_execution_forms.md) |
| `core_nature/axis_spec.md` | 各分類軸的應對措施：規範、metadata、library（三階段進行中）→ 現行見 [doc_05_axes_metadata](doc_05_axes_metadata.md) |
| `core_nature/lib_spec.md` | Python library API 設計細節（register()、--metadata 攔截、各軸 metadata fields）→ 現行見 [doc_06_lib_contract](doc_06_lib_contract.md) |

> 此檔本身僅是切分指標，實質內容已併入 `core_nature/` 規範家族（現由 doc_ 系列承載）。保留於此是為記錄「thinking_core_nature → core_nature/ 切分」這一演化事件。
