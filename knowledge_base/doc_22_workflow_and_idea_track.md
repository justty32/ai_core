# doc_22 — 工作流 slash command 與點子捕捉軌（dogfood）

> **來源**：`.claude/commands/`（resume / test / spec / proto / intake / critique / expand）＋ `ideas/`（raw/cleaned/notes/brainstorm 骨架）＋ `try_implement/tools/idea.py`（dogfood 工具）＋ `CLAUDE.md`「工作流 Slash Command」節
> **狀態**：① 現行/操作面（slash command 與 `ideas/` 軌已 commit、在用）；其中 **LLM 加工層為 ② 原型 dogfood**（走 `idea.py` 打真 API，規範未定）。
> **一行摘要**：把本 repo 的循環工作拆成「每工作流一指令」的 slash command；並從 TTemp 借來「點子捕捉／頭腦風暴軌」，其 LLM 加工**不派 Claude Code agent，改 shell out 給 `idea.py` 打真 API**——這是 roadmap「廉價小模型消費者」的第一個真實 dogfood。

相關交叉引用：`idea.py` 工具細節見 [code_02_prototype_tools.md](code_02_prototype_tools.md)；其底層真 backend / socket 傳輸見 [code_03_prototype_lib.md](code_03_prototype_lib.md)；Gap G 與 DECISIONS E 區見 [note_06_decisions_and_open_questions.md](note_06_decisions_and_open_questions.md)；願景「資產工廠→笨模型消費者」見 [note_01_vision_and_origin.md](note_01_vision_and_origin.md)；第九軸 `nondeterministic` 見 [doc_05_axes_metadata.md](doc_05_axes_metadata.md)。

> **時間線**：本軌於 **2026-06-08** 自 TTemp 同步並 ai_core 特化（commit `82f8e6f`→`0a3bef6`）。KB 主體（2026-06-07）建立**之後**才加入，故本檔是該批工作的整合記錄。

---

## 1. 為什麼有這一軌

兩件不同的事被同一個「同步 TTemp 模式」帶進來：

1. **工作流 slash command** — 把本 repo 自身的循環工作（接續、測試、規範扶正、原型探索）各做成一個 slash command，打一句即「變身」該模式。指令只是**模式切換與重點摘要**，權威來源仍是 `CLAUDE.md` 與對應文件。
2. **點子捕捉／頭腦風暴軌** — 一條與程式碼／規範工作**並行、互不干擾**的軌：把口述靈感落檔、清稿、彙整、找漏洞、擴展。產物統一落 `ideas/`。

關鍵的 ai_core 特化：點子軌的 LLM 加工原本（在 TTemp）靠 Claude Code **agent**（甚至派背景 subagent）。本 repo 把那層 agent 換成 **shell out 給 `try_implement/tools/idea.py` 打真 API**——即用 ai_core 自己的 LLM 基礎設施去消費，**dogfood**。

---

## 2. 七個 slash command（`.claude/commands/`）

| 指令 | 用途 | 權威來源 |
|------|------|---------|
| `/resume` | 接續工作：依固定閱讀順序重建上下文、給下一步 | `CLAUDE.md`「接續工作的閱讀順序」、`roadmap.md`、`progress.md` |
| `/test` | 跑全部測試（pytest + 兩個 smoke test）並回報 | `CLAUDE.md`「建置／測試指令」 |
| `/spec` | 規範扶正：把 `try_implement/` 提案收斂進 `core_nature/` 與 `_core.py` | `DECISIONS.md`、`core_nature/` |
| `/proto` | 原型探索：在 `try_implement/` 先寫出來跑跑看、暴露設計缺口 | `try_implement/README.md` |
| `/intake` | 口述線一條龍：原文逐字→初步整理→匯總筆記（落 `ideas/`） | 指令內含鐵則 |
| `/critique` | 頭腦風暴・找漏洞 → `ideas/brainstorm/` | 指令自含 |
| `/expand` | 頭腦風暴・擴展靈感 → `ideas/brainstorm/` | 指令自含 |

前四個（resume/test/spec/proto）是 repo 自身工作的模式切換；後三個（intake/critique/expand）是點子捕捉軌。

---

## 3. 點子捕捉軌的資料流與 `ideas/` 骨架

`ideas/` 是四個並列子目錄（各帶 `.gitkeep` 佔位）：

```
ideas/
├── raw/         # WF1 口述原文逐字（一字不改）
├── cleaned/     # WF2 初步整理（去語音錯字/贅字/缺字，最大保留原意）
├── notes/       # WF3 匯總筆記（結構化）
└── brainstorm/  # WF4 找漏洞 / WF5 擴展的產物
```

工作流階段（WF 編號沿用 TTemp）：

| 階段 | slash command | idea 子命令 | 性質 | 落點 |
|---|---|---|---|---|
| WF1 原文逐字 | `/intake` | （ingest 內含，不過 LLM）| 一字不改 append | `ideas/raw/` |
| WF2 初步整理 | `/intake` | `idea clean` | 純 filter（過 LLM）| `ideas/cleaned/` |
| WF3 匯總筆記 | `/intake` | `idea notes` | 純 filter（過 LLM）| `ideas/notes/` |
| WF4 找漏洞 | `/critique` | `idea critique` | 純 filter（過 LLM）| `ideas/brainstorm/` |
| WF5 擴展 | `/expand` | `idea expand` | 純 filter（過 LLM）| `ideas/brainstorm/` |

**`/intake` 的執行模式特化**：只有 `/intake`（語音輸入友善）保留「**主 agent 即時回應**」模式，但實際動作丟的是**背景 Bash 呼叫 `idea ingest`**（口述一條龍 orchestrator，自己寫檔），而非派背景 subagent。`/critique`、`/expand` 則**同步 shell out** 給 `idea critique`／`idea expand`。

**鐵則 1（主題隔離 + raw 不可變）**：`raw/` 一字不改、不經 LLM；同一主題（slug）跨多則口述沿用同一份檔，讓 raw 累積、cleaned/notes 整份刷新。slug 比對用**嚴格整檔名**（`<時間戳>-<slug>.md`）而非 glob，避免某 slug 撞進「以它為連字號後綴」的另一主題檔（審查發現的污染 bug，commit `8e77f54` 修）。

---

## 4. dogfood 串接：第一個真實「廉價小模型消費者」

`idea.py` 把點子軌的 LLM 加工從「派 agent」換成「打真 API」，**串起 ai_core 三元件成完整一條**：

```
bind() 打包（元件 2，lib/llm_call）
  → LLM Entry Manager 單例守門 + consume rate（元件 1，tools/llm_entry_manager）
    → 真 backend（OpenAIBackend / AnthropicBackend，lib/llm_call）
      → 真 API
```

這正是 roadmap 願景的具體落地：成熟期的系統由「廉價小模型消費者」去用上層生出的資產，本軌是第一個真實串接。

- **backend 路由**：預設**經由 LLM Entry Manager**（尊重「LLM 是 singleton 資源、集中守 consume rate」立場）；`--direct` 跳過 entry manager 直接呼叫 backend（測試/不需 rate 管理時）。
- **打哪個真 LLM**：由環境變數 `AI_CORE_LLM_PROVIDER` / `AI_CORE_LLM_BASE_URL` / `AI_CORE_LLM_MODEL`（＋ `API_KEY` / `MAX_TOKENS`）決定（見 `lib/llm_call.backend_from_env`）；**未設定則 EchoBackend 回顯**，離線可跑、煙霧測試友善。
- **第九軸誠實宣告**：每個 LLM 子命令（clean/notes/critique/expand/ingest）的 `--metadata` 都宣告 `nondeterministic: true`——這正是 LLM 工具在治理原則下的預設身分：**尚未領證書前，誠實標記自己會吐隨機**。

> 連線模式（socket vs subprocess）與 consume rate 跨呼叫累計（Gap G 修復）詳見 [code_02_prototype_tools.md](code_02_prototype_tools.md) 的 `idea.py` 與 `llm_entry_manager.py` 節。

---

## 5. 這一軌暴露/修復的設計缺口

| 缺口 | 性質 | 狀態 |
|---|---|---|
| **Gap G** — one-shot 工具經 entry manager 時 consume rate 無法跨呼叫累計 | subprocess 模式每次新開 entry manager process，RateMeter 從零開始 | **✅ 已修（2026-06-08）**：`lib/server.serve_socket` + `llm_entry_manager --socket` 長駐單例，多 caller 共用同一 RateMeter。詳見 note_06。 |
| **E1** — `OpenAIBackend` 送 `max_tokens` | 對本地 ollama/llama.cpp/vLLM 正確，但 OpenAI 官方新模型（o1/o3）要求 `max_completion_tokens` | 低風險，留待 v0 真接官方新模型時依 provider 分流欄位 |
| **E2** — `serve_socket` serial-accept 無逾時 | 單一 idle 連線會卡死整個 daemon | 歸入「persistent singleton 如何被多 caller 共用」同一懸案，v0 正式化 |
| **E3** — `_slugify` regex `一-鿿` 冗餘 | Python `\w` 已含 CJK，純清理無行為影響 | 順手清理 |

（E1/E2/E3 = DECISIONS E 區；整合於 note_06。）

---

## 6. 與 KB 其他層的關係

- 本軌**與程式碼／規範工作並行、互不干擾**：它消費 ai_core 的 LLM 基礎設施（元件 1+2），但不動軸定義、不動 `_core.py` 契約。
- 對 KB 整體而言：slash command 與 `ideas/` 是**操作面新增**（① 現行）；`idea.py` 與其依賴的真 backend / socket 傳輸是 **② 原型 dogfood**，扶正成規範前仍待定奪。
- 它把「roadmap 願景」與「已實作零件」第一次接成一條可跑的鏈，故也是 [doc_20_taming_framework.md](doc_20_taming_framework.md)（馴化框架）「契約層 bind」在真實任務上的首次應用。
