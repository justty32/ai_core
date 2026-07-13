# doc_10_arch_overview

- **來源**：`docs/architectures/01_overview.md` ＋ `docs/ARCHITECTURE.md`（快速概覽四元件表）、`docs/architectures/index.md`（導覽結構）
- **狀態**：舊版架構文件（部分已被 core_nature/roadmap 取代）
- **一行摘要**：另一版的 ai_core 架構總綱——定位（AI 80% / 人類 20%）、設計原則、Singleton Resource Manager Pattern（兩種亞型）、整體架構圖、技術選型。
- **與現行差異提示**：
  - 此版以「五大 CLI 元件」（`ai-core-server`／`ai-core-call`／`ai-core-hub`／`ai-core-sfc`／`ai-core-author`）為骨幹，與 core_nature 的「library `register()`/`intercept()` + 九軸」描述體系不同。現行正式核心只有 `src/ai_core/_core.py`，並無這些 CLI 二進位。本版的元件命名與設計細節僅作為「另版架構提案」保留。
  - 此版技術選型大量引入外部相依（`litellm`、`fastapi`+`uvicorn`、`uv`、`platformdirs`），**與現行硬約束「只用 Python 3.11+ 標準庫、`dependencies = []`」直接衝突**。以 roadmap/CLAUDE.md 的「Least dependency / 純標準庫」為準。
  - metadata 協議差異見 [doc_11_arch_protocol.md](doc_11_arch_protocol.md)；現行九軸見 [doc_05_axes_metadata.md](doc_05_axes_metadata.md)、library 契約見 [doc_06_lib_contract.md](doc_06_lib_contract.md)。
- **交叉引用**：本架構家族其餘檔案 [doc_11_arch_protocol.md](doc_11_arch_protocol.md)、[doc_12_arch_entry_manager.md](doc_12_arch_entry_manager.md)、[doc_13_arch_hub.md](doc_13_arch_hub.md)、[doc_14_arch_author_sfc.md](doc_14_arch_author_sfc.md)、[doc_15_arch_project_milestones.md](doc_15_arch_project_milestones.md)、[doc_16_arch_agents.md](doc_16_arch_agents.md)、[doc_17_arch_changelog.md](doc_17_arch_changelog.md)。願景脈絡見 [note_01_vision_and_origin.md](note_01_vision_and_origin.md)。

---

## 快速概覽（出自 `ARCHITECTURE.md`）

ai_core 把 LLM 呼叫視為**函式**，再把所有支援設施（queue 管理、shell 包裝、metadata 發現）疊在這個基礎概念上。四個核心元件：

| 元件 | CLI | 說明 |
|---|---|---|
| LLM Entry Manager | `ai-core-server` + `ai-core-call` | 統一管理多個 LLM 後端，queue + rate limit |
| Function Hub | `ai-core-hub` / `ai-core-hub-server` | 掃描函式集，產生 AI 可用的 skill 清單 |
| Small Function Center | `ai-core-sfc` | 把大量微型函式集中到一個 dispatcher，避免清單膨脹 |
| ai-core-author | `ai-core-author` | 一站式產生新函式（generate → dry-run → register） |

**唯一跨元件硬規則**：每個函式支援 `--metadata`，回傳合法 JSON。

> 導覽索引（`index.md`）對應章節：01_overview = 定位 + §1–§3；02_protocol = §4–§5；03_entry_manager = §6；04_hub = §7；05_author_sfc = §8–§9；06_project = §10–§14；07_agents = §15；08_changelog = §16。

---

## 定位（Positioning）

ai_core 同時服務 **AI agent** 與 **人類使用者**。預期使用比例約 **AI 80% / 人類 20%**，但**人類保持完全掌控**：

- AI agent 透過 ai_core 呼叫 LLM、組合既有函式、甚至**產出新的函式**
- 函式會自我增殖：人類或 AI 在「足夠明確的 context」下，呼叫某 authoring function 讓 LLM 寫出新 function；這個新 function 進入 hub 被後續 caller 使用，形成生長的工具樹
- 人類看到 AI 寫出的好用 function 可以直接拿來用 — 所有函式都是普通 shell command，沒有 AI-only 的格式

設計後果：

1. CLI 與 metadata 同時對人類友善與 AI 友善 — 不為了討好任一方而犧牲另一方
2. 系統不假設 caller 是 AI；任何 function 都能在純 shell 環境下被人手動呼叫
3. 任何 AI 寫出來的 function 都得通過 dry-run + 範例驗證才能進入 registry，避免「AI 生 AI 用」的內部黑箱

---

## §1. 設計原則

承襲 `thinking.md`：

- **KISS、輕量、不重造輪子、相依最少**
- **shell 為一等公民**：所有元件以 CLI 為主介面，人類與 AI 都用同一個介面
- **每個函式預設 stateless one-shot**；需要狀態者自己管狀態，重量級者升格為常駐 server
- **錯誤訊息走 stderr、正常輸出走 stdout 或 `--output` 指定路徑**（Unix 慣例）
- **metadata 是極簡協議**：只強制「合法 JSON」，不規定具體 schema
- **metadata 容錯**：所有 caller / hub / author 等元件都必須**預期某些函式根本沒寫 `--metadata`、或寫得不合法、或缺慣例 key**。這時應 graceful 降級（使用合理預設、提示使用者讀文件），而不是 crash 或拒絕呼叫該函式

### Singleton Resource Manager Pattern

對「一次只能服務一個請求、建立成本高」的單例資源（LLM 模型、heavy GPU 計算、配額受限的外部 API、長連線的本地服務等），統一採用此 pattern：

1. **常駐 server** 包住該資源，HTTP / IPC 對外
2. **per-resource FIFO queue + worker**
3. **配套一支 shell wrapper CLI**（因為 shell 不該被迫去 curl）— wrapper 把 args 翻成 HTTP 呼叫；server + wrapper 是**共生雙元件**，一起發布
4. wrapper CLI 必須提供 `--metadata`（描述這支 wrapper 工具自身）與 `--entry-metadata`（描述 server 底下個別 entry，如某個 model）
5. 配額不足 / 不合法輸入 / 不存在的 entry → wrapper 直接 stderr + 非 0 exit code

**此 pattern 有兩種亞型：**

| 亞型 | 說明 | 介面 |
|---|---|---|
| **Entry-managing singleton** | 管理多個具名資源（entries），每個 entry 有自己的配額 / 狀態。例：LLM Entry Manager（多個 model） | `--metadata`（工具自身）+ `--entry-metadata`（個別 entry） |
| **Simple singleton server** | 包住單一資源，不需要 entry 概念。功能上更像常駐的 MCP 工具伺服器，只是變成 singleton 以保護底層資源。 | 只有 `--metadata` |

只有 **entry-managing** 亞型需要在 metadata 中宣告 `has_entries: true`，讓 caller 知道還有 `--entry-metadata` 可用（詳見 [doc_11_arch_protocol.md](doc_11_arch_protocol.md) §4.7）。

**LLM Entry Manager 是 entry-managing 亞型的首個實例**：`ai-core-server`（server）+ `ai-core-call`（wrapper CLI）。未來新增同類 manager 時應沿用同一套介面慣例（同名旗標、同樣的錯誤碼語意），讓 caller 對所有 manager 的行為一致。

> 寫第二個 manager 時的 server / queue / rate-limit 共用樣板會抽到 `src/ai_core/protocol/`，見 [doc_15_arch_project_milestones.md](doc_15_arch_project_milestones.md) §10.1。第一版 YAGNI，先讓 Entry Manager 內部消化。

---

## §2. 整體架構

```
caller（人類 shell / AI agent / 其他 function）
   │
   ├── 直接 shell call ────────────────────────────┐
   │                                                ▼
   │                               任意語言寫的 function
   │                               （唯一硬規則：--metadata 印合法 JSON）
   │                                       │
   │                                       ├── 單純函式：直接執行
   │                                       └── 重資源：透過 wrapper CLI ──▶ singleton server
   │
   ├── 想知道有哪些 function ──▶ Function Hub（server / one-shot 雙形態）
   │
   └── 想新增 function ─────────▶ ai-core-author（一站式產生 + dry-run + 註冊）
```

具體 singleton manager 的首例：

```
ai-core-server（HTTP）          ←── ai-core-call（shell wrapper CLI）
   ├─ per-model asyncio queue
   ├─ rate limit (rpm / tokens / cost)
   └─ litellm router → ollama / lm-studio / gemini
```

---

## §3. 技術選型

> ⚠️ **與現行衝突**：下表的外部相依（litellm / fastapi / uvicorn / uv / platformdirs）違反現行「只用標準庫、`dependencies = []`」硬約束。保留作為舊版提案紀錄。

| 項目 | 選擇 | 理由 |
|---|---|---|
| 主語言 | Python 3.11+ | LLM 生態最完整；asyncio 跨平台行為穩定 |
| 套件管理 | `uv` | 快、跨平台、有 lockfile |
| LLM 抽象 | `litellm` | 已支援 ollama、lm studio (OpenAI 相容)、gemini |
| HTTP server | `fastapi` + `uvicorn` | Entry Manager / Hub server 都用 |
| 跨平台路徑 | `platformdirs` | 自動找對 config / data dir |
| CLI 註冊 | `pyproject.toml [project.scripts]` | 不靠 shebang，Win/Linux 都能跑 |
| 配置 / metadata | JSON | 已拍板 |

> **語言中立性**：Python 是 ai_core **自身實作**所用的語言。**Function 本身語言不限** — bash、Python、Go、任何可執行檔皆可，只要符合 `--metadata` 協議即可被 hub 收錄與呼叫。
