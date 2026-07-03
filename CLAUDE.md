# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 專案狀態（2026-05-29 更新）

此專案**已脫離純構想階段**：已有正式核心 library、一整套設計規範、探索性原型實作與全綠測試。不要再把它當成「只有 `thinking.md` 的空專案」。

當前形態分三層（理解這個分層是讀懂本 repo 的關鍵）：

| 層 | 位置 | 性質 | 可信度 |
|---|---|---|---|
| **北極星 / 戰略** | `roadmap.md` | 為什麼做、終點長什麼樣、第一目標問題、v0 切片 | 主文件，判斷「該不該做、先做哪個」的尺 |
| **規範 (spec)** | `core_nature/` | 設計模式的精確定義（八/九軸、複合、CLI、library 契約） | **已扶正的內容是定案**；標「待填／待議」者未定 |
| **原型 (探索)** | `try_implement/` | 「先寫出來跑跑看」的遊樂場，暴露設計缺口 | **多數是提案**，不是定案；扶正狀態見其 `DECISIONS.md` |

正式核心程式碼只有一處：`src/ai_core/_core.py`（＋ `tests/test_core.py`）。`try_implement/` 下的東西即使能跑，也**尚未**等於規範定案。

> 早期的 `thinking*.md`（`thinking.md`、`thinking_layers.md`、`thinking_oop.md` …）是歷史思考留存，已被 `roadmap.md` 與 `core_nature/` 取代為當前事實來源；讀它們是為了理解演化脈絡，不是為了取得現況。**（2026-07-02 已移入 `archive/thinking/`，連同舊版單頁 `archive/overview.html`；根目錄不再放這些。）**

## 設計哲學

整個系統遵循四個原則：

- **KISS** — 簡單優先
- **Lightweight** — 輕量
- **No wheel-remake** — 不重造輪子
- **Least dependency** — 相依最少化

這不是美學偏好，而是**前提的邏輯必然**（見 `roadmap.md §1`）：本專案假設未來只剩便宜的本地小模型可用，一個「窮」的專案付不起龐大基礎設施的維護成本。當你考慮要不要引入框架、抽象層、或自訂 DSL 時，預設答案應該是「不要」，除非有明確理由。

實作時的硬約束：**只用 Python 3.11+ 標準庫**（argparse / json / subprocess / pathlib / urllib…），無外部相依。`pyproject.toml` 的 `dependencies = []` 是刻意維持的。目標平台為 **POSIX（含 pipeline）；Windows 不在考慮範圍**。

## 北極星：這個專案到底要做什麼

（完整論述在 `roadmap.md`，以下是判斷取捨時必須先內化的精華。）

- **初心**：AI 泡沫化會讓聰明可靠的 LLM 變貴 → 假設只剩便宜本地 <10B 小模型 → 做一套框架**輔助小笨模型**，把它包裝成可靠的東西。CLI 天生適合 LLM。
- **時間維度**：趁現在聰明模型還便宜，用它當**資產工廠**一次性生出耐久資產（程序 / snippet / few-shot / 骨架），讓未來的**笨模型消費者**去用。飛輪第一圈要趁早轉。
- **終點形狀**：成熟系統裡 LLM 退縮到只剩兩個介面——(1) 人類意圖 → 程式可理解意圖的**翻譯層**；(2) 尚未固化的**模糊前沿暫時處理者**。其餘全是確定性程式碼。
- **治理原則（拒絕為預設 + 憑證准入）**：LLM 預設零領地；要佔據某環節必須通過「**必要性**（確定性程式碼辦不到）＋**穩定性**（測試組下夠穩）」兩道閘門，並領一張證書（用哪個模型、測試組、穩定度 %）。可被撤照。這是第九軸 `nondeterministic` 的由來。
- **第一目標問題**：**程式碼輔助助手**——上層聰明模型「讀框架 → 生資產」（結構化抽取）、下層笨模型「在精準標記位置填碼」（受約束生成）。
- **方法論剎車**：先選目標問題，再讓它告訴你哪些規範該先定。**目標問題 = 停止鍵**，是「東西愈做愈大」的解藥。

## 目錄結構

```
ai_core/
├── roadmap.md              # ★ 北極星 / 戰略主文件（回來先讀這個）
├── src/ai_core/_core.py    # ★ 唯一的正式核心：register()/intercept() + 九軸驗證
├── tests/test_core.py      # 正式核心的測試
├── core_nature/            # 規範家族（spec）
│   ├── overview.md         #   本質定位（部分待填）
│   ├── terminal_model.md   #   從 terminal 出發的執行模型
│   ├── execution_forms.md  #   執行形式（one-shot / persistent …）
│   ├── axis_spec.md        #   描述函式特性的「軸」規範
│   ├── composite_spec.md   #   複合 / 組合規範
│   ├── cli_spec.md         #   CLI 介面契約
│   ├── lib_spec.md         #   library 契約（register/intercept 落地處）
│   └── data_format.md      #   資料格式
├── try_implement/          # 探索性原型遊樂場（多為提案，非定案）
│   ├── DECISIONS.md        # ★ 待定奪的規範決策清單（回來次讀這個）
│   ├── README.md           #   原型總覽與各檔職責
│   ├── tools/              #   indexer/router/switch/sfc/hub/llm_entry_manager/chain/idea
│   ├── lib/                #   複合規範參考實作 + 基礎設施 + llm_call
│   ├── demos/              #   3 個可跑 demo
│   ├── smoke_test.py       #   83 項斷言
│   └── lib_smoke_test.py   #   78 項斷言
├── funcs/                  # 範例函式（echo.sh …）
├── progress.md             # 接續上次工作的 resume 指標
├── ideas/                  # 點子捕捉／頭腦風暴軌（intake/critique/expand 產物，借自 TTemp）
├── archive/                # 歷史留存（已被取代、僅供追溯）：thinking/*.md ×10 + 舊版 overview.html
├── .claude/commands/       # 循環工作的專屬 slash command（見下「工作流 slash command」）
└── pyproject.toml          # hatchling 打包；無 runtime 相依
```

## 工作流 Slash Command（`.claude/commands/`）

把本 repo 的循環工作拆成專屬 slash command，打一句即「變身」該模式。指令只是模式切換與重點摘要，**權威來源仍是本檔與對應文件**。

| 指令 | 用途 | 權威來源 |
|------|------|---------|
| `/resume` | 接續工作：依固定閱讀順序重建上下文、給下一步 | 本檔「接續工作的閱讀順序」、`roadmap.md`、`progress.md` |
| `/test` | 跑全部測試（pytest + 兩個 smoke test）並回報 | 本檔「建置／測試指令」 |
| `/spec` | 規範扶正：把 `try_implement/` 提案收斂進 `core_nature/` 與 `_core.py` | `DECISIONS.md`、`core_nature/` |
| `/proto` | 原型探索：在 `try_implement/` 先寫出來跑跑看、暴露設計缺口 | `try_implement/README.md` |
| `/intake` | 口述線一條龍：原文逐字→初步整理→匯總筆記（落 `ideas/`） | 指令內含鐵則 |
| `/critique` | 頭腦風暴・找漏洞 → `ideas/brainstorm/` | 指令自含 |
| `/expand` | 頭腦風暴・擴展靈感 → `ideas/brainstorm/` | 指令自含 |

`/intake`、`/critique`、`/expand` 是從 TTemp 借來的**點子捕捉／頭腦風暴軌**，產物統一落在 `ideas/`（`raw/cleaned/notes/brainstorm`），與本 repo 的程式碼／規範工作並行、互不干擾。

**ai_core 特化（2026-06-08）**：這三個指令的 LLM 加工**不派 Claude Code agent，改 shell out 給 `try_implement/tools/idea.py` 打真 API**（dogfood ai_core 自己的 LLM 基礎設施）。其中只有 `/intake`（語音輸入）保留「**主 agent 即時回應**」模式，但動作丟的是**背景 Bash 呼叫 `idea ingest`**（非背景 subagent）；`/critique`、`/expand` 則同步 shell out 給 `idea critique`／`idea expand`。要接真 LLM 需設 `AI_CORE_LLM_PROVIDER/BASE_URL/MODEL` 環境變數，未設則 EchoBackend 回顯（測試用）。

## 建置 / 測試指令

```bash
# 環境：repo 內已有 .venv（Python 3.14）。安裝開發相依：
.venv/bin/pip install -e ".[dev]"

# 跑正式核心測試（src/ai_core + tests/）
.venv/bin/python -m pytest -q                 # 目前 101 passed

# 跑原型煙霧測試（不需 pytest，純標準庫）
.venv/bin/python try_implement/smoke_test.py      # 83 項斷言
.venv/bin/python try_implement/lib_smoke_test.py  # 78 項斷言

# 跑原型工具（範例）
.venv/bin/python try_implement/tools/hub.py --metadata
.venv/bin/python try_implement/tools/chain.py ...
```

## 核心契約：`--metadata` 與 register/intercept（已定案）

這是**跨元件的硬契約**，Function Hub / Indexer 的可行性完全建立在它之上。實作任何新函式時，metadata 不是可選項。

`src/ai_core/_core.py` 提供的 API（純宣告 / 顯式攔截拆分模型）：

- `register(**meta)` — 宣告程式頂層 metadata（dispatcher 預設行為）。**純宣告、零副作用**：不讀 argv、不攔截、不 exit。
- `register_subcommand(name, **meta)` — 宣告靜態子命令的 scoped metadata（解「單一執行檔多種 lifecycle」）。
- `register_subcommand_resolver(fn)` — 動態子命令解析（名稱來自外部資料，如 SFC store）。
- `intercept(argv=None)` — 在 `main()`/`__main__` 顯式呼叫，命中 `--metadata`（含 `<sub> --metadata`、`--store` 前綴）則輸出 JSON 並 exit，否則交還控制權。

**重要慣例**：`register*` 系列因為無副作用，import 即安全；但仍應在 `__main__`/`main()` 呼叫。`--metadata` 的生效靠顯式 `intercept()`，不靠 import 副作用。

**九個軸**（`_KNOWN_FIELDS`）：`entries`、`lifecycle`（one_shot/persistent）、`state`（stateless/stateful_external）、`state_dirs`、`resources`、`interruptible`、`guarantee`（none<idempotent<transactional）、`dry_run`、`nondeterministic`（第九軸：`true`＝未認證隨機 / `{model,test_set,stability}`＝證書）。

**九軸之外（2026-07-03，「回到初心」近期焦點①②落地）**：頂層另有**非軸欄位 `reliability`**（0.0–1.0 數值或 `{value,...}` 帶 provenance；隨觀測更新的活值，與證書 `stability` 凍結值分工）；entry 屬性新增 **`format`**（`json`/`ndjson` 或 dict 逃生口）與 **`schema`**（JSON Schema dict，描述單筆輸出結構＝接縫 typing ＋日後 deopt guard 的原料）。非文字內容由既有 `type: {"base":"binary","mime":...}` 涵蓋。詳見 `lib_spec.md`。

函式型態原則仍成立：**多數函式 one-shot、stateless**；需多輪互動者**自行管理外部狀態**；**重量級函式變 server**（與 LLM Entry Manager 同單例模式）。**shell 為一等公民**——不要為了 Python API 好用而犧牲 CLI 清晰度。

## 規劃中 / 原型中的五大元件

概念定義見 `roadmap.md` 與下列原型，狀態如下：

1. **LLM Entry Manager** — 統一 LLM 呼叫入口（類 litellm / OpenRouter）。LLM 是**單例資源**（一次一請求）→ 佇列模式；集中管理 **consume rate**（token / 金錢 / 本地 GPU）。原型：`try_implement/tools/llm_entry_manager.py`。**backend 已改由 `backend_from_env()` 依環境變數挑真 LLM**（CLI `--provider/--model/--base-url` 可覆寫）。**`--socket <path>` 可長駐成 Unix socket daemon**：多個 one-shot caller 連同一個、共用 RateMeter → consume rate 跨呼叫累計（已修 Gap G；底層為 `lib/server.serve_socket`）。
2. **LLM Calling Packing** — 把 `llm_call(string)->string` 疊 context binding 與 post-processing 成具語意函式。原型：`try_implement/lib/llm_call.py`。**真 backend 已實作**：`OpenAIBackend`（OpenAI 相容 `/chat/completions`，吃本地 ollama/llama.cpp/vLLM/OpenRouter）、`AnthropicBackend`（`/v1/messages`），都走 `lib/call.Http`（urllib，零相依）。

> **點子捕捉軌的 dogfood（2026-06-08）**：`try_implement/tools/idea.py` 把 `/intake /critique /expand` 的「派 Claude Code agent」換成「打真 API」——子命令 `clean/notes/critique/expand`（純 filter）＋ `ingest`（口述一條龍），預設經元件 1 entry manager 路由，串起 `bind`（元件2）→ entry manager（元件1）→ 真 backend → API。每個 LLM 子命令宣告第九軸 `nondeterministic:true`。這是 roadmap「廉價小模型消費者」的第一個真實串接。
3. **Shell / App 作為函式** — 文字進文字出，shell 是最自然封裝；每個函式都實作 `--metadata`。契約已落地（見上節）。
4. **Function Hub** — 掃描函式集、呼叫各 `--metadata`、彙整成「給 LLM 的 skill 清單」，含 context budget 逐級收斂。原型：`try_implement/tools/hub.py`（規範未定，原型自定義）。
5. **Small Function Center (SFC)** — 把大量 tiny function 集中到一個 git-style subcommand dispatcher，避免檔案爆炸。原型：`try_implement/tools/sfc.py`（已改接真 `ai_core`）。

## 進行中的工作與待決事項

- **已收斂進規範（2026-05-26）**：A1/A2/A3（register 宣告/攔截拆分）、第九軸 `nondeterministic`、`memoized` 不入軸（維持純 runtime）。詳見 `try_implement/DECISIONS.md` 頂部「✅ 已收斂」。
- **刻意暫緩至 v0**：A4（組合軸推導）、B 系列（B1 語意/用途欄位、B2 共用模組、B3 沙箱）。依 `roadmap.md §7`，留給 v0 切片去逼出優先序。
- **待使用者追認**：`DECISIONS.md` 的 D 區（我先前自行拍板的 7 項）。
- **最硬的未決題**：固化引擎手動 vs 自動（`roadmap.md §3.6 / §8`，標「這題優先」）。
- **尚未動工**：`roadmap.md §6` 的 **v0 最小垂直切片**（一個真實框架 → 一次聰明模型生資產 → 笨模型 + 行數助手 + retry/guard 做出 raw 笨模型做不到的正確程式碼編輯）。

**接續工作的閱讀順序**：先讀 `roadmap.md` → 再依當下焦點讀 `try_implement/DECISIONS.md`（懸案清單）或 `core_nature/composite_spec.md`（規範現況）。

## 與上層 `pas/` 專案的關係

本資料夾位於 `pas/others/ai_core/`。上層 `pas/CLAUDE.md` 定義了 pas 工作空間的規範（繁體中文輸出、自動留檔到 `analysis/<project_name>/` 等）。**這些規範對本資料夾下的工作同樣適用**，除非本檔案另有指定。

## 文件語言

所有回覆、註解、留檔請使用**繁體中文**。程式碼識別子、shell 指令、技術名詞保留原文。

## 維護本檔的義務

本 repo 演進很快（規範會扶正、原型會被刪/扶正、v0 會開工）。當你改動了會讓上面任何一節失準的東西——尤其是**核心 API、軸定義、目錄結構、測試數量、扶正/待決狀態**——請**同步更新本檔**。
