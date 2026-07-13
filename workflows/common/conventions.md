# 設計哲學 + 核心契約 + 程式碼慣例（碼相關工作流共用）

← [common/README](README.md)｜[INDEX](../../INDEX.md)

碰原始碼的工作流（spec / proto / feature-dev …）共用這套。純文檔類工作流用不到。結構整理原則（被動）在 [DEV-GUIDE](../../DEV-GUIDE.md)；always-on 鐵律在 [AGENTS.md](../../AGENTS.md)。

---

## 設計哲學

整個系統遵循四個原則：

- **KISS** — 簡單優先
- **Lightweight** — 輕量
- **No wheel-remake** — 不重造輪子
- **Least dependency** — 相依最少化

這不是美學偏好，而是**前提的邏輯必然**（見 [roadmap.md](../roadmap.md) §1）：本專案假設未來只剩便宜的本地小模型可用，一個「窮」的專案付不起龐大基礎設施的維護成本。當你考慮要不要引入框架、抽象層、或自訂 DSL 時，預設答案應該是「不要」，除非有明確理由。

實作時的硬約束：**只用 Python 3.11+ 標準庫**（argparse / json / subprocess / pathlib / urllib…），無外部相依。`pyproject.toml` 的 `dependencies = []` 是刻意維持的。目標平台為 **POSIX（含 pipeline）；Windows 不在考慮範圍**。

---

## 核心契約：`--metadata` 與 register/intercept（已定案）

> **權威來源＝ [workflows/spec/lib_spec.md](../spec/lib_spec.md)**（＋ `axis_spec.md`）。本節是給碰碼工作流的速查摘要，細節與最新狀態以 lib_spec.md 為準。

這是**跨元件的硬契約**，Function Hub / Indexer 的可行性完全建立在它之上。實作任何新函式時，metadata 不是可選項。`sub_projs/ver_1/src/ai_core/_core.py` 提供的 API（純宣告 / 顯式攔截拆分模型）：

- `register(**meta)` — 宣告程式頂層 metadata（dispatcher 預設行為）。**純宣告、零副作用**：不讀 argv、不攔截、不 exit。
- `register_subcommand(name, **meta)` — 宣告靜態子命令的 scoped metadata（解「單一執行檔多種 lifecycle」）。
- `register_subcommand_resolver(fn)` — 動態子命令解析（名稱來自外部資料，如 SFC store）。
- `intercept(argv=None)` — 在 `main()`/`__main__` 顯式呼叫，命中 `--metadata`（含 `<sub> --metadata`、`--store` 前綴）則輸出 JSON 並 exit，否則交還控制權。

**重要慣例**：`register*` 系列因為無副作用，import 即安全；但仍應在 `__main__`/`main()` 呼叫。`--metadata` 的生效靠顯式 `intercept()`，不靠 import 副作用。

**九個軸**（`_KNOWN_FIELDS`）：`entries`、`lifecycle`（one_shot/persistent）、`state`（stateless/stateful_external）、`state_dirs`、`resources`、`interruptible`、`guarantee`（none<idempotent<transactional）、`dry_run`、`nondeterministic`（第九軸：`true`＝未認證隨機 / `{model,test_set,stability}`＝證書）。

**九軸之外（2026-07-03）**：頂層另有**非軸欄位 `reliability`**（0.0–1.0 數值或 `{value,...}` 帶 provenance；隨觀測更新的活值，與證書 `stability` 凍結值分工）；entry 屬性新增 **`format`**（`json`/`ndjson` 或 dict 逃生口）與 **`schema`**（JSON Schema dict，描述單筆輸出結構＝接縫 typing ＋日後 deopt guard 的原料）。非文字內容由既有 `type: {"base":"binary","mime":...}` 涵蓋。詳見 lib_spec.md。

**函式型態原則**：多數函式 one-shot、stateless；需多輪互動者**自行管理外部狀態**；**重量級函式變 server**（與 LLM Entry Manager 同單例模式）。**shell 為一等公民**——不要為了 Python API 好用而犧牲 CLI 清晰度。

---

## 規劃中 / 原型中的五大元件（濃縮）

概念定義見 [roadmap.md](../roadmap.md)，狀態與原型路徑如下：

1. **LLM Entry Manager** — 統一 LLM 呼叫入口（類 litellm/OpenRouter）；LLM 是單例資源 → 佇列模式，集中管理 consume rate。`--socket` 可長駐成 Unix socket daemon 跨呼叫累計 RateMeter。原型：`sub_projs/ver_1/try_implement/tools/llm_entry_manager.py`。
2. **LLM Calling Packing** — 把 `llm_call(string)->string` 疊 context binding 與 post-processing 成具語意函式。真 backend 已實作 `OpenAIBackend`（OpenAI 相容）與 `AnthropicBackend`，都走 `lib/call.Http`（urllib，零相依）。原型：`sub_projs/ver_1/try_implement/lib/llm_call.py`。
3. **Shell / App 作為函式** — 文字進文字出，shell 是最自然封裝；每個函式都實作 `--metadata`（契約見上節）。
4. **Function Hub** — 掃描函式集、呼叫各 `--metadata`、彙整成「給 LLM 的 skill 清單」，含 context budget 逐級收斂。原型：`sub_projs/ver_1/try_implement/tools/hub.py`（規範未定）。
5. **Small Function Center (SFC)** — 把大量 tiny function 集中到 git-style subcommand dispatcher，避免檔案爆炸。原型：`sub_projs/ver_1/try_implement/tools/sfc.py`（已接真 `ai_core`）。

> **點子捕捉軌 dogfood**：`sub_projs/ver_1/try_implement/tools/idea.py` 串起 `bind`（元件2）→ entry manager（元件1）→ 真 backend → API，是「廉價小模型消費者」的第一個真實串接。詳見 [idea-capture 工作流](../idea-capture.md)。

---

## 程式碼慣例

- **零外部相依**：新程式碼只用 Python 3.11+ 標準庫（見鐵律 1）。引入任何第三方套件前必須有明確理由並先確認。
- **shell 一等公民**：函式以 CLI 為主介面，每個函式實作 `--metadata`；不為了 Python API 好用而犧牲 CLI 清晰度。
- **單檔行數門檻** 300 行（與 [DEV-GUIDE](../../DEV-GUIDE.md) 觸發 A 一致）；本質不可分的單體核心／規範（如 `_core.py`、`lib_spec.md`）可超標保留。
- **改動軸定義／核心 API／測試數量時**：同步更新 `workflows/spec/` 對應規範、[workflows/testing.md](../testing.md) 的測試數字，與相關導航 index（鐵律 5）。
