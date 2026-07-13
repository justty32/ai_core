# note_07 — 舊版設計世系：系統設計三世代

> - **來源**：git commit `684082c` 之 `SYSTEM_DESIGN.md` + `SYSTEM_DESIGN_EXTRA.md` + `old/SYSTEM_DESIGN_1.md` + `old/SYSTEM_DESIGN_2.md` + `old/SYSTEM_DESIGN_3.md`
> - **狀態**：**舊版設計世系**（已於 commit `7b9b8e3`「以 ai_core 框架/規範版完全覆蓋本 repo」被完全覆蓋；僅供溯源，非現行事實）
> - **一行摘要**：舊版 ai_core 的設計演化三世代——從「LISP 純函數 + BaseFunction/Registry/Server 重抽象」（old/1、old/2、old/3）演進到「process + `--metadata` 一條約定、依託 OS、AI 自我擴展」（SYSTEM_DESIGN.md、EXTRA）。
> - **現行事實見** [note_01_vision_and_origin.md](note_01_vision_and_origin.md) 與 [doc_05_axes_metadata.md](doc_05_axes_metadata.md)、[doc_10_arch_overview.md](doc_10_arch_overview.md)、[doc_20_taming_framework.md](doc_20_taming_framework.md)。

---

## ⚠️ 閱讀前必讀：這是歷史，不是現況

本檔保留的是 ai_core **已被完全覆蓋的舊版設計**。它與現行設計有大量名稱衝突（同名異義）與理念分歧。**凡與現行衝突，一律以現行為準**。最重大的分歧整理於本檔末節「舊版 vs 現行：重大設計分歧」。

舊版設計本身就分成「兩個世代」——這點對理解很關鍵：

- **舊版的「舊版」（`old/` 目錄，2026-04-25 前後）**：重抽象路線。`BaseFunction` 抽象類、`FunctionRegistry`、`CoreEnvironment`、`Client-Server`、LISP S-expression 組合、依賴 LiteLLM。
- **舊版的「現版」（`SYSTEM_DESIGN.md` / `EXTRA`，2026-04-26）**：對上述路線的自我否定，回歸「process + `--metadata`」極簡約定，棄用所有重抽象。

即使是舊版「現版」這個 process 路線，也已被 commit `7b9b8e3` 的現行框架/規範版（roadmap.md + core_nature/ + 九軸 + 拒絕為預設治理）整個取代。

---

# 第一部分：舊版「現版」——Process-Based Architecture
> 出自 `SYSTEM_DESIGN.md`（updated 2026-04-26，標「主要設計文檔」）與 `SYSTEM_DESIGN_EXTRA.md`（設計總結與待決策）

## 核心理念（舊版「現版」）

ai_core 是一個極簡的 AI 系統，核心三句話：

```
process = 可執行文件 + --metadata 約定
hub     = 一次性的索引工具
session = 一個有持久化的 dict
AI      = 持續生成新 process 來擴展系統
```

明確宣示「沒有 BaseFunction，沒有 Registry class，沒有 server，沒有 REST API」——這正是對 `old/` 世代的否定。

**為什麼選 process？** 「OS 已經有完美的 process 管理機制」：
- 函數定義 → executable file
- 函數參數 → CLI args
- I/O → stdin / stdout
- 註冊表 → 文件系統
- 組合 → pipe / subprocess
- 元數據 → `--metadata` 約定

**唯一的共同約定**（像 `--help` / `--version` 的 CLI convention）：
```bash
./any_process --metadata    # 輸出 JSON，描述自己
./any_process "input"       # 正常執行
```

**AI 自我擴展是核心驅動力**：「約定越簡單 → AI 實作門檻越低 → 系統擴展越快」。整個系統只有 `--metadata` 一條規則，是刻意的——讓 AI 寫 process 的負擔最小。

> 註：「AI 自我擴展」概念在現行已被更精細的「拒絕為預設 + 憑證准入」治理取代；現行不鼓勵 AI 無門檻佔據環節。

## 系統元件（舊版「現版」）

### 1. Process
最基本單位。任何遵守 `--metadata` 約定的可執行文件。

舊版 **Metadata 格式**（注意與現行九軸完全不同）：
```json
{
  "name": "translate",
  "description": "將文字翻譯成目標語言",
  "version": "1.0",
  "tags": ["translate", "language"],
  "input": "text",
  "output": "text"
}
```

**I/O 約定**：小輸入用 CLI arg；大輸入用 file → stdin；大輸出用 stdout → file。

舊版獨有的 **Process 五型（依實作方式分）**：
- **LLM 入口** — 直接呼叫 LLM（如 `gemini-2.5-flash.py`）
- **Context 綁定包** — 預設 system prompt 後呼叫 LLM 入口（如 `senior-engineer.py`）
- **輸出解析包** — 解析或轉換另一個 process 的輸出（如 `arrange-output.py`）
- **組合 process** — 串接多個 process（內部 subprocess 或讓使用者用 shell pipe）
- **任何其他** — shell script、純計算、外部工具 wrapper 等

> 對照現行：現行的型態分類用「九軸」（lifecycle/state/guarantee/nondeterministic…），與此五型分類無對應關係。

### 2. Hub
一次性工具，**不是 server**：
```bash
./hub --build-list ./processes/   # 掃描目錄，呼叫每個 --metadata，寫入 list.json
./hub --search-func "translate"   # 讀 list.json，回傳匹配的 process
```
- **list.json** 用 atomic write（先寫 tmp 再 rename）。
- **為什麼不是 server**：無狀態、無 socket/port/並發問題、失敗代價小、文件系統即儲存。
- **未來**：`--search-func` 目前精確/關鍵字匹配，未來可加模糊/語意（LLM 或向量）。

### 3. Session Library
幫 process 管理多輪 session 狀態，**運作模型像回合制 RPG**：
```
使用者行動（輸入文字、按 Enter）
  → 機器開始執行（呼叫 process、LLM、運算）
  → 機器完成，等待下一輪使用者行動 → 重複
```
邊界：✅ 只負責 dict 讀寫與持久化；❌ 不處理 UI 邏輯（slash command、args、file save UI 屬上層）。最簡實作就是一個會把 dict 寫回 JSON 的 `Session` 類。

### 4. cli_lib（未來）
建在 session library 之上的互動層：slash command（`/reset`、`/save`、`/history`）、args 支援、file save UI、big I/O control。可選引入。

### 5. func_center（未來）
輕量 server，管理簡單的 LLM+context 包，避免每次啟動 Python interpreter 的開銷：
```bash
./func_center --func llm_summarize "input text"
```

## 舊版「現版」設計原則
✅ 依託 OS ✅ 約定極簡 ✅ 無狀態核心 ✅ AI 自我擴展 ✅ 回合制模型 ✅ 延遲優化（func_center / cli_lib 都是「之後」）

## SYSTEM_DESIGN_EXTRA.md 獨有內容

### 從前一版（old/）演進的對照表
| 之前（old/ 世代） | 現在（process 世代） |
|------|------|
| BaseFunction 類別 | 任何 executable file |
| FunctionRegistry 類別 | 文件系統 + list.json |
| Closure / Context 物件區分 | process 自己決定 |
| Server 持續運行 | hub 一次性執行 |
| REST API / Module / Pipe | stdin / stdout / args |
| Layer 1 / 2 / 3 | 一個約定（--metadata） |

「關鍵頓悟：OS 已經有完美的函數機制。」

### 已決策清單（2026-05-18 端到端驗證後）— 舊版定案
- **`--metadata` schema**：必填 `name`、`description`；選填 `version`、`tags`、`input`、`output`。缺欄位的 process 被跳過並寫 stderr；未知欄位給 warn-only 提示。
- **失敗格式**：non-zero exit + stderr；stdout 只給成功結果（`chain.py` 依賴此契約）。
- **大輸入觸發**：小字串（單行、<4KB）用 CLI args；多行/大文字用 stdin；process 必須同時支援兩者：`text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()`。
- **模糊搜尋**：先 defer，現為大小寫不敏感子字串匹配（name+description+tags 合併）。空 query 直接 error。
- **processes 目錄結構**：扁平，44 個還可控；超過 ~100 再分子目錄。
- **list.json 版本管理**：不做（`--build-list` 是一次性 atomic rename，重跑等於 reset）。
- **session 持久化**：每次 `set()` 立即寫盤（atomic rename）。
- **多 process 共享 session**：不支援，每個 process 自己擁有 session 文件。
- **session 位置慣例**：`~/.ai_core/<process_name>/session.json`。
- **AI 生成 process 的標準測試流程（六步）**：① `hub --search-func` 確認不重複 → ② 寫 `processes/<name>.py`（含 METADATA + `--metadata` 入口 + stdin/args 雙入口）→ ③ `chmod +x` → ④ `hub --validate <path>` → ⑤ 跑煙霧測試 → ⑥ `hub --build-list` 重建索引。

### 明確棄用清單（舊版「不再追求的東西」）
❌ 統一 BaseFunction 類別 ❌ 中央 FunctionRegistry ❌ Closure vs Context 形式區分 ❌ Server 持續運行（除 func_center 未來）❌ 多種通信協議 ❌ 複雜依賴解析/版本管理 ❌ 跨機器分散式。

### 「已吸收」（2026-05-18，源自兄弟版本 ai_core/）— 六項
比對 `../pas/others/ai_core` 那份更繁複的版本後，挑了六項與「無狀態核心」相容者納入：
1. **`io` 結構化欄位**（選填，與 `input`/`output` 字串並存）：`{"io": {"input": "stdin|file|args|none", "output": "stdout|file|none", "format": {"input": "text|json|binary", "output": "..."}}}`，`--validate` 對未知值 warn-only。
2. **metadata 容錯降級而非 skip**：`list.json` 每筆有 `_status: "ok|partial|absent"`；`partial`/`absent` 含 `_warning`；`search-func` 跳過 `absent` 仍返回 `partial`。
3. **examples + 自動 dry-run**：`--validate` 把 `metadata.examples` 逐一餵 stdin 比對 stdout（當回歸測試）。
4. **`--json-errors` 全域旗標**：stderr 改吐 `{"type","message","hint","retriable"}` 單行 JSON。
5. **`hub --export <fmt>`**：第一版支援 `openai-tools` / `anthropic-tools`。
6. **`auto/AGENTS.md` + `auto/FUNCTIONS.md` 自動產出**：`--gen-agents-md` / `--gen-functions-md`。

**舊版明確不採用**：常駐 LLM entry server、queue、rate limit、token auth、author 一站式 dry-run pipeline、Small Function Center dispatcher——理由是「這些都假設框架是中心，與 OS 是中心、hub 一次性衝突」。

> ⚠️ 注意此處與現行的劇烈反轉：舊版**明確拒絕**的「常駐 LLM entry server、queue、rate limit、Small Function Center dispatcher」，正是**現行五大規劃元件**（LLM Entry Manager、Function Hub、SFC…）。詳見末節分歧表。

---

# 第二部分：old/SYSTEM_DESIGN_1.md — Advanced Concepts
> 出自 `old/SYSTEM_DESIGN_1.md`，標 type=project，自述「衍生層，更基礎概念應在 system_design_2」。**全屬重抽象世代，多為待決策腦力激盪。**

## 1. S-Expression 風格的函數組合（Chained Calling）
不同於線性管道，主張**樹狀/圖狀**函數組合，採 S-expression 形式。

LISP 風格表示：
```lisp
(call_llm
  (contextualize
    (denoise_voice tokens metadata)
    session_id)
  user_profile)
```
嵌套字典（Python）表示：用 `{"func": ..., "args": [...]}` 巢狀，變數以 `$tokens` / `$metadata` / `$session_id` / `$user_profile` 表示。

特點：多輸入函數、樹狀執行（順序由依賴決定）、並行可能性。
待決策：樹狀結構如何評估（自上/自下）、`$變數` 解析來源、哪些可並行、失敗如何恢復。

## 2. 調度與資源管理（OS-Style Scheduling）
背景：LLM 處理耗時（秒到數十秒），不應阻塞其他會話。主張像 **OS 進程調度器**一樣工作（多會話時間片示意）。

**任務狀態機**（舊版反覆出現的核心構件）：
```
pending ──→ running ──→ waiting ──→ completed
            │                        ↑
            └────→ suspended ────────┘
```
- pending=等待調度；running=在 CPU 上執行；waiting=等資源（如 LLM API 回應）；suspended=被調度器暫停讓出 CPU；completed=完成。

**時間統計用途**：函數 metadata 的時間統計用來做**預測性調度**（短操作立即執行；長操作如 LLM 檢查配額與優先級）。

**利用 OS 機制（不自己實現調度器）**三方案：A 多進程 `multiprocessing`（真並行、開銷大）；B 非同步 `asyncio`（輕量、單核）；C Coroutine（最輕量、需 runtime 支持）。資源限制用 `asyncio.Semaphore(N)` 限制 LLM 同時請求數。

## 3. 函數定義 vs 實例化分離
採編譯/運行時分離模型：**Definition Time**（函數類定義含邏輯與 metadata）→ **Instantiation Time**（每次執行創建新實例，類似 OS 創進程）→ **Runtime**（實例執行）→ **Cleanup**（釋放資源）。

範例以 `function_definitions["denoise_voice_v1"]()` 為各 Session 創建獨立實例（各有自己的棧/本地變數，互不污染）。
優點：重用性（定義共用、省記憶體）、隔離性、資源管理、真並行、狀態追蹤。

## 4. 利用操作系統系統函數（對照表）
| OS 機制 | Python 實現 | 用途 |
|--------|----------|------|
| 進程調度 | `multiprocessing` | 真正的並行 |
| 事件循環 | `asyncio` | 輕量任務調度 |
| Coroutine | `async/await` | 語言級任務管理 |
| 信號量 | `threading.Semaphore` | 資源限制 |
| 互斥鎖 | `threading.Lock` | 同步訪問 |
| 管道 | `os.pipe()` | 進程間通信 |
| 共享記憶體 | `multiprocessing.SharedMemory` | 進程間數據共享 |

設計原則：「讓 OS 做它擅長的事，我們專注於業務邏輯。」

## 5. 集成流程圖（舊版完整設想）
```
S-expression 組合 → 評估樹狀結構 → 生成任務圖（DAG）→ 調度器決策
（短任務→立即執行；長任務→檢查資源條件執行；並行任務→用 OS 機制並行）
→ 實例化函數 → 執行（同步/異步）→ 狀態轉移 → 清理資源
```
仍待決策：S-expression 評估方向、`$變數` 解析、並行粒度、資源配額、錯誤恢復、可觀測性。

---

# 第三部分：old/SYSTEM_DESIGN_2.md — Complete Core Architecture
> 出自 `old/SYSTEM_DESIGN_2.md`（updated 2026-04-25）。**舊版最完整、最重抽象的一份**，自述「核心代碼 ~200 行」。

## 願景：LLM 即純函數 + 三層核心抽象（舊版最具代表性的獨特構想）
基本哲學：把 **LLM 視為純函數 `llm : tokens → tokens`**；「沒有魔法，只有 closure 與 transform」。

三層 closure 抽象（**make_model → bind → take**，舊版招牌）：
- **`make_model(model, temperature, top_k, ...)`** — 把模型名與參數封進 closure，回傳純 `tokens → tokens` 函數。
- **`bind(context, model_fn)`** — 偏函數應用，把 system prompt / few-shot 預先綁入，回傳更專化的函數。
- **`take(schema, fn)`** — 解析輸出，把原始 token 轉成結構化資料。

組合範例：
```python
agent = take(json_schema,
         bind(system_prompt,
         make_model("claude-sonnet-4-6", temperature=0.7, top_k=40)))
result = agent("使用者的輸入")
```

## 極簡架構（舊版自稱）：核心四職責 + 唯一依賴 LiteLLM
關鍵轉變：「不要把工具/框架/Agent 納入核心，而是看作外部『左右臂』，透過標準 OS 層通信。」核心只保留：Task Router、Session Manager、Function Registry、LiteLLM Client（唯一框架依賴）。

**核心四大職責**：
1. **Task Router** — `determine_function(tokens, metadata) -> str`，唯一智能決策點，可用 LiteLLM 做簡單分類（範例用 `if "optimize" in text` 等啟發式）。
2. **Session Manager** — `sessions: session_id → context`，`get_or_create_session` / `cleanup_session`。
3. **Function Registry** — `register(func_id, func_config)`，config 含 `type: shell|python|api|llm` 與 `path`/`endpoint`/`model`/`instance`。
4. **LiteLLM Client** — 「核心唯一的框架依賴」，統一 LLM 調用接口。

舊版自稱優勢：極簡（~200 行）、單一依賴、松耦合、易擴展、成本透明、語言無關、輕量部署。

## 函數的四種類型（依執行載體分；與第一部分的「五型」不同分類）
1. **Shell 命令（外部進程）** — 輸入 JSON（命令行參數），輸出 JSON（stdout）；核心用 `subprocess.run(["bash", path, json.dumps(task)])` 執行。
2. **本地 Python 函數（進程內）** — 繼承 `BaseFunction`，`__call__(tokens, metadata) -> (tokens, metadata)`；config `type: python` + `instance`。
3. **外部 API（框架微服務）** — 框架（LangChain/DSPy）作為獨立 FastAPI 服務，核心用 `requests.post(endpoint, json=task)` 調用。
4. **LiteLLM 調用** — config `type: llm` + `model` + `params`，核心呼 `llm_client.call(...)`。

## 通信方式 & OS 層（四種）
1. **Shell（進程 + JSON）**：語言無關，但進程/序列化開銷。
2. **IPC（Pipe/Queue）**：給長期運行工具（如 Aider）用 `multiprocessing.Pipe()`；低開銷、支持流式，但需維護工具進程生命週期。
3. **HTTP API（微服務）**：完全解耦、易部署，但有網絡延遲。
4. **文件系統（簡單狀態共享）**：寫 `task.json` → 工具處理 → 寫 `result.json` → 核心讀；最簡單但效率低。

## 多客戶端架構（Token + Metadata 統一接口）
核心**完全解耦於具體 I/O 實現**；CLI / Desktop / Web / Android 各自處理 I/O，透過統一 `{tokens, metadata}` 與核心通信。

**Input Metadata 設計**（獨特欄位）：`source`(voice/text/file/api)、`client_id`、`session_id`、`user_signal`(important/normal/background)、`timestamp`、`context_objects`、`encoding`、`source_characteristics`(has_noise/language/confidence)。

**Output Metadata 設計**（獨特欄位）：`format`、`priority`、`urgency`(immediate/batch/deferred)、`target_session`、`recommended_clients`、`voice_length`、`importance_score`、`can_batch`、`ttl`、`requires_user_attention`。

多會話並行示例：使用者同時開 Desktop(primary) / Web(secondary_1) / CLI(secondary_2)，輸入流帶 session_id 進核心，輸出流依 priority 分發到不同客戶端。

## Layer 1 數據結構
- **通信格式**：`Request = {tokens: bytes, metadata: dict}`、`Response = {tokens: bytes, metadata: dict}`。
- **Token 表示 bytes**：理由「更接近原始文本、支持多內容、分詞責任交給 LLM、客戶端自選粒度」。
- **Metadata 用 dict 而非 dataclass**：理由「dict 夠靈活、可動態加欄位、符合 LISP 精神（數據與代碼對稱）」。函數可動態加 `denoised`、`confidence` 等欄位。

## Layer 1 函數定義：BaseFunction 基類（舊版重抽象核心）
`BaseFunction(ABC)` 帶 `id`/`type`/`version`、`resource_profile`（memory: self/running/peak；time: startup/actual_work/teardown）、可發現性 metadata（grouping/tags/expanded_name）、`input_requirements`（source_type/required_fields/preconditions）、`output_produces`。
- 抽象方法簽名：`__call__(tokens: bytes, metadata: dict) -> (bytes, dict)`。
- `should_execute(metadata)`：檢查 source_type 與必需欄位，決定是否執行（**舊版 self-filtering 機制**）。
- `get_signature()`：回傳函數簽名資訊。
- 具體範例 `DenoiseVoiceFunction`：`source_type="voice"`、`output_produces["denoised"]=True`、`resource_profile` 標 5MB / 100ms。

## 函數元數據與自動組合（舊版招牌構想之一）
元數據多維度：資源消耗（resource_profile）、可發現性（id/type/expanded_name/tags/grouping）、語義分類（conceptual_purpose/grouping/domain）。

**自動函數組合系統**（自動鏈接）：
```
Single Root Input (tokens + metadata)
  → Function Registry (all functions with metadata)
  → Automatic Composition Engine
  → Optimized Function Chain (auto-linked by output→input matching)
  → Execution Pipeline (sync + chained)
```
核心概念：「投資」建很多小函數（清晰簽名 + 完整 metadata）、「自動鏈接」（輸出類型匹配下一函數輸入類型）、「智能選擇」（多函數符合時依資源/語義優先級選）。
待釐清：多選擇決策邏輯、鏈終止條件、環形依賴處理。

## 執行模型 & 調度（V1→V4 路線圖）
- **V1（當前）：同步 + 串聯**——`for func in func_chain: tokens, metadata = func(tokens, metadata)`。
- **函數鏈選擇**：依 `metadata.source` 查預定義鏈，例：`voice` → `[denoise_voice_v1, contextualize_v1, call_llm_v1, format_output_voice_v1]`；`text` / `file` 各有預定義鏈；再用 `should_execute` 過濾。
- V1 特性：同步、串聯（UNIX pipe 哲學）、自動選擇。
- **路線圖**：V1 同步串聯 → V2 並行（DAG、多鏈並行、多會話）→ V3 非同步（async/await、高吞吐）→ V4 動態調度（依負載選策略、自適應、分佈式）。
- 任務狀態機同 old/1（pending→running→waiting→completed，可 suspended）。

## 會話管理：CoreEnvironment（LISP 風格執行環境）
類似 LISP REPL，維護全局函數註冊表 + 多會話本地上下文 + 執行狀態。獨特機制：`type_index` / `tag_index` 提供 O(1) 按類型/標籤查找；`register_function` 同步建索引；`get_or_create_session` / `cleanup_session`；`process(request)` 主入口（會話管理 → 選鏈 → 執行 → 視 `session_end` 清理）。

## 實現優先級（週計畫）
- **P1（第一週）**：BaseFunction 基類、CoreEnvironment、簡單選鏈、同步串聯執行。
- **P2（第二週）**：具體函數（DenoiseVoice / Contextualize / CallLLM / FormatOutput）、單元測試、metadata 設計驗證。
- **P3（第三週）**：多會話管理、性能測試、決策是否優化為非同步。
- **P4（後續）**：DAG 執行、動態函數選擇（性能/偏好/AB）、分佈式執行。

## 設計原則（六條）
LISP 風格、元數據驅動、簡潔至上、Python 優先（後期可遷 Rust）、測試驅動、極簡核心。

## 成本管理（舊版立場：核心不管成本）
核心**不**統計資源/算成本/管預算/追蹤消費；交由**用戶**監控（範例 `UserCostManager` 包裝核心調用並計數 litellm/api/shell）。

## 框架的位置（完全外部）
LangChain / DSPy / CrewAI → 作為 HTTP Service 部署，核心 API 調用；Aider → Shell/subprocess 調用；**LiteLLM → 唯一在核心內的框架**。

## 完整核心代碼框架（`core.py` 極簡版）
舊版附了完整 `core.py`：`BaseFunction(ABC)` + `CoreEnvironment`（`register_function` / `register_function_object` 建 type/tag 索引、`process_request`、`_execute_function` 依 type 分派到 `_call_shell` / `_call_python` / `_call_api` / `_call_llm`、`_determine_function` 啟發式路由、`_get_or_create_session` / `_cleanup_session`）。依賴 `subprocess` / `json` / `requests` / `litellm`。

> ⚠️ 此 `core.py` 與現行唯一正式核心 `src/ai_core/_core.py`（`register()` / `intercept()` + 九軸驗證、零外部相依）**完全無關**——同名「core」但設計、依賴、API 全異。

## 結語（舊版承諾）
「把 LLM 視為純函數，通過元數據驅動的自動組合，在多客戶端架構下，以簡潔優雅的方式架構 AI 應用。」核心特性：極簡（~200 行）、單一依賴（LiteLLM）、松耦合、易擴展、成本透明、語言無關、開放生態。

---

# 第四部分：old/SYSTEM_DESIGN_3.md — Context Management Core
> 出自 `old/SYSTEM_DESIGN_3.md`（updated 2026-04-25）。從**會話上下文管理**角度切入的核心設計。

## 設計出發點
不從架構圖而從**實際使用場景**出發：使用者在終端輸入文字、同時多會話（`/session XXX` 切換）、每會話有獨立對話歷史/臨時數據/本地函數定義、所有會話共享 LLM client/全局函數/系統資源。
核心問題：**多會話如何各管上下文不互相干擾，同時共享單例資源（尤其 LLM）？** 理念類比：全局函數=系統庫、會話=進程、LLM client=CPU（共享資源但先不做複雜調度）。

## Session：會話三層上下文（獨特設計）
`Session(session_id, persistent)` 含：
- **Layer 1: history** — LLM 對話歷史 `[{"role","content"}]`（長度影響 context 成本）。
- **Layer 2: local_functions** — 本地函數環境，**可覆蓋全局函數**。
- **Layer 3: local_vars** — 臨時變量/狀態（如 `/set var value`）。
- metadata: created_at / last_accessed / state(active/suspended/destroyed/persisted)。

## 函數查找層級：Lexical Scope（舊版獨特機制）
`resolve_function` 查找順序：① 會話 local_functions（最高優先，可 **shadow** 全局）→ ② global_functions → ③ 報 `FunctionNotFoundError`。類比 Python 作用域（local → global → built-ins）。不支援跨會話函數調用（會話隔離）。

## CoreEnvironment（此版）
管理 `global_functions` + 單例 `llm_client = LiteLLMClient()` + `sessions` + `active_session_id`。提供 `create_session` / `switch_session`（切換時把舊會話標 suspended）/ `process`。

## LLM 作為共享單例
`call_llm(user_input, session)`：把輸入加入該會話 history → 用會話完整 history 呼 LLM（`model="claude-sonnet-4-6"`）→ 回應追加回 history。**單例 client、會話私有 history**。

## 會話生命週期
```
create → active ←→ suspended（切換到另一會話）
                → destroyed（臨時會話，進程結束）
                → persisted（持久會話，寫磁盤）
```
臨時會話 `persistent=False`（進程結束消失）；持久會話 `persistent=True`（`save_session`/`load_session`）。

## Context 長度警告機制（舊版獨特立場：只警告不截斷）
核心**只負責監控**：`_estimate_tokens`（粗估＝字符數/4）→ 超閾值就在 output metadata 附警告，**不自動截斷或 summarize**（讓用戶決定）。建議用戶命令：`/compress`（LLM 摘要舊對話）、`/clear`（清空）、`/forget N`（遺忘最舊 N 條）。

## 待決策問題（old/3）
持久化存儲格式（JSON/SQLite/MessagePack？如何序列化函數對象）、函數定義語言（只 Python？還是 Shell/其他？如何註冊自定義）、會話間是否共享狀態（目前完全隔離）、LLM 並發調用（排隊 vs 併發）。

設計原則：簡潔、隔離、共享、可恢復、用戶中心。

---

# 舊版 vs 現行：重大設計分歧（供主代理寫入總索引）

| 維度 | 舊版設計世系（本檔） | 現行（以此為準） |
|---|---|---|
| **核心隱喻** | LLM 即純函數 `tokens→tokens`；make_model→bind→take 三層 closure（old/2） | LLM 退縮為「意圖翻譯層 + 模糊前沿暫時處理者」，其餘是確定性程式碼（roadmap） |
| **核心程式碼** | `core.py`：BaseFunction(ABC) + CoreEnvironment，依賴 litellm/requests（old/2） | `src/ai_core/_core.py`：`register()`/`intercept()` + 九軸，**零外部相依** |
| **metadata 模型** | `name/description/version/tags/input/output`（+ 後加 `io`/`examples`） | **九軸**：entries/lifecycle/state/state_dirs/resources/interruptible/guarantee/dry_run/nondeterministic |
| **依賴立場** | old/ 世代依賴 LiteLLM（「唯一框架依賴」）、requests | **只用 Python 3.11+ 標準庫，`dependencies = []`** |
| **AI 的角色** | AI 自我擴展為核心驅動力，門檻越低越好 | **拒絕為預設 + 憑證准入**：LLM 預設零領地，需過必要性+穩定性兩道閘門領證 |
| **組合模型** | S-expression 樹/DAG 自動組合引擎、自動鏈接（old/1、old/2） | 複合/組合規範（core_nature/composite_spec），shell pipe 為一等公民 |
| **常駐 server / queue / rate limit / SFC** | 舊版「現版」**明確拒絕**（與「OS 為中心」衝突） | 現行**規劃中的五大元件**正包含 LLM Entry Manager(queue/rate)、Function Hub、SFC dispatcher |
| **多客戶端 + bytes token + 豐富 I/O metadata** | old/2 大篇幅設計（priority/urgency/voice_length…） | 現行未採此路線；資料格式見 core_nature/data_format |
| **成本管理** | 核心不管，交用戶（old/2 UserCostManager） | LLM Entry Manager 集中管理 consume rate（token/金錢/GPU） |
| **調度** | OS-style 任務狀態機 + V1→V4（同步→DAG→async→動態調度） | 不在現行主線；現行強調 one-shot / persistent lifecycle 軸 |
| **平台** | old/2 含 Windows-agnostic 多客戶端（Android 等） | **POSIX only，Windows 不在考慮範圍** |

> 一句話總結分歧：舊版是「**把 LLM 當函數、用重抽象/自動組合/多客戶端把它包成系統**」（old/ 世代）以及其反動「**只靠一條 `--metadata` 約定 + AI 自我擴展**」（SYSTEM_DESIGN.md 世代）；現行則是「**拒絕為預設的治理 + 九軸契約 + 零相依標準庫核心**」，並且把舊版「現版」明確拒絕的常駐元件重新納為規劃。

舊版的 README / usage / architecture 見 [note_08_legacy_readme_usage_arch.md](note_08_legacy_readme_usage_arch.md)；舊版的 43~44 個 process 清單與 agents 生態見 [note_09_legacy_processes_agents.md](note_09_legacy_processes_agents.md)。
