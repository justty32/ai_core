# note_08 — 舊版設計世系：README / usage / architecture

> - **來源**：git commit `684082c` 之 `README.md` + `usage.md` + `architecture.md`（外加舊 `CLAUDE.md` 的願景段，作為對照）
> - **狀態**：**舊版設計世系**（已於 commit `7b9b8e3` 被完全覆蓋；僅供溯源，非現行事實）
> - **一行摘要**：舊版的「使用方式」與「架構視角」——process + `--metadata` 約定、hub 一次性索引、session 回合制、AI 六步擴展流程、hub 完整 CLI 命令、planner+chain 編排、元件邊界與不變式。
> - **現行事實見** [note_01_vision_and_origin.md](note_01_vision_and_origin.md) 與 [doc_05_axes_metadata.md](doc_05_axes_metadata.md)、[doc_10_arch_overview.md](doc_10_arch_overview.md)。本檔同屬 [note_07_legacy_system_design.md](note_07_legacy_system_design.md) 的舊版世系。

---

## ⚠️ 閱讀前必讀
本檔是舊版 ai_core（process 世代）的操作面文件。其 `--metadata` schema、hub 命令、目錄結構**皆已被現行覆蓋**。凡與現行（roadmap.md / core_nature/ / 九軸）衝突，以現行為準。重大分歧表見 [note_07_legacy_system_design.md](note_07_legacy_system_design.md) 末節。

---

# 一、舊 CLAUDE.md 的願景與核心理念（對照用）

舊版專案願景三句：
```
process = 可執行文件 + --metadata 約定
hub     = 一次性的索引工具
session = 一個有持久化的 dict
AI      = 持續生成新 process 來擴展系統
```
「沒有 BaseFunction 類別，沒有 Registry class，沒有 server，沒有 REST API。把 OS 已有的 executable / CLI args / stdin-stdout / 文件系統當成函數系統來用。」

舊版**必要元件**：process、hub（`--build-list` 掃描、`--search-func` 查詢）、session library（回合制 dict 持久化）。
舊版**未來元件**：cli_lib（slash command/args/UI/big I/O control）、func_center（輕量 LLM 包的 server，避免 interpreter 啟動開銷）。

舊版設計原則（打勾）：依託 OS、約定極簡、AI 自我擴展、無狀態核心、延遲優化、回合制模型。
使用者資訊：繁體中文母語、語音輸入只支援英文、一律以繁體中文回覆。

---

# 二、README.md（舊版）

## Setup（注意：舊版需 LiteLLM，與現行零相依相反）
- 需 Python 3.10+（測試在 3.12）；`python -m venv .venv` + `pip install -r requirements.txt`。
- **重要**：所有命令用 `.venv/bin/python` 跑——hub 會把 `sys.executable` 傳給子 process，讓 process 拿到 venv 裡的依賴（`litellm` 等）。
- LLM process 需設 API key，例：`export GEMINI_API_KEY=...`（預設 model `gemini/gemini-2.5-flash`）或 `LLM_MODEL=openai/gpt-4o-mini` + `OPENAI_API_KEY=...`。

## 快速試用
```bash
.venv/bin/python hub --build-list ./processes/        # 1. 建索引
.venv/bin/python hub --search-func "translate"        # 2. 搜尋
echo "Hello, World!" | .venv/bin/python processes/slugify.py   # 3. 跑 process → hello-world
.venv/bin/python -m pytest tests/ -q                  # 4. 跑測試
```

## Process 約定：`--metadata` 必填/選填表（舊版 schema，非現行九軸）
| 欄位 | 必填 | 說明 |
|------|------|------|
| `name` | ✅ | process 名稱（與檔名一致） |
| `description` | ✅ | 一句話描述功能 |
| `version` | | 預設無 |
| `tags` | | 字串陣列，幫助搜尋 |
| `input` | | 輸入型態描述 |
| `output` | | 輸出型態描述 |
| `examples` | | `[{"input","output"}]`；`hub --validate` 會逐一執行並比對輸出 |
| `io` | | `{"input": stdin\|file\|args\|none, "output": stdout\|file\|none, "format": {"input": text\|json\|binary, "output": ...}}`；給 orchestrator/export 用 |

**錯誤與 I/O 契約**：成功 exit 0 寫 stdout；失敗 non-zero exit 寫 stderr；小輸入用 CLI args；大/多行輸入用 stdin，process 必須兩種都能讀：
```python
text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
```

**最小骨架**（舊版範式）：含 `METADATA` dict + `if "--metadata" in sys.argv: print(json.dumps(METADATA))` 否則執行 `run()`。

## AI 寫 process 的標準流程（六步）
```bash
./hub --search-func "<keyword>"                     # 1. 確認沒重複
# 2. 寫 processes/<name>.py（依骨架）
chmod +x processes/<name>.py                        # 3. 設執行權限
.venv/bin/python hub --validate processes/<name>.py # 4. 驗證契約 + 跑 examples
echo "..." | .venv/bin/python processes/<name>.py   # 5.（可選）煙霧測試
.venv/bin/python hub --build-list ./processes/      # 6. 重建索引
```
`hub --validate` 檢查：執行權限、`--metadata` 能跑、輸出是合法 JSON 物件、含必填欄位、退出碼 0。若含 `examples` 則逐一把 input 餵 stdin、比對 output（examples 即自動煙霧測試）。

## Hub 完整命令（舊版 CLI 介面）
```bash
hub --build-list <dir> [--output <file>]    # 掃描目錄建索引
hub --search-func <query> [--list <file>]   # 子字串搜尋 (name/description/tags)
hub --validate <process_path>               # 檢查契約 + 跑 examples（若有）
hub --gen-agents-md [--output <file>]       # 產出給 AI 讀的入口文件
hub --gen-functions-md [--output <file>]    # 產出完整 process 清單
hub --export <fmt> [--output <file>]        # 輸出 tool schemas: openai-tools | anthropic-tools
```
- 預設 `<file>` 是 `./list.json`，預設 `<dir>` 是 `./processes`。
- 所有 `--gen-*` / `--export` 無 `--output` 時直接印 stdout（pipe-friendly）。
- **全域旗標 `--json-errors`**：把 stderr 錯誤改成單行 JSON `{"type","message","hint","retriable"}`，給 orchestrator（planner/chain）程式化處理。
- **list.json 條目格式**：每 entry 含 `_status: "ok|partial|absent"`；`partial`＝metadata 解析了但缺必填；`absent`＝`--metadata` 整個失敗；search-func 跳過 `absent` 但仍返回 `partial`。

## 編排：planner + chain（舊版兩個 meta-process）
```bash
# planner：自然語言任務 → JSON 執行計劃
.venv/bin/python processes/planner.py "review my Go code and write release notes"
# chain：吃 planner 的 JSON，依序執行每一步
.venv/bin/python processes/planner.py "..." | \
  .venv/bin/python processes/chain.py --input my_code.go
```
每一步中間結果存到 `~/.ai_core/chain/session.json`。

## 舊版目錄（README 版）
```
ai_core/
├── hub                 ← 索引工具（可執行）
├── list.json           ← hub --build-list 的輸出
├── processes/          ← 所有 process（扁平、44 個）
├── auto/               ← 自動產出：AGENTS.md / FUNCTIONS.md / tools.*.json
├── session_lib/        ← Session library 原始碼
├── tests/              ← pytest 測試
├── SYSTEM_DESIGN.md / SYSTEM_DESIGN_EXTRA.md / architecture.md / usage.md / CLAUDE.md
```

---

# 三、usage.md（舊版，使用者視角）

核心概念：所有功能都是 process，process 就是一個可執行文件（shell script / .py / binary），`process = 可執行文件 + CLI args`。

## Process 範例（usage.md 獨有的四種具體寫法）
- **LLM 入口** `gemini-2.5-flash.py`：直接 `litellm.completion(model="gemini/gemini-2.5-flash", temperature=1.2)`，吃 argv/stdin，print 結果。
- **Context 綁定包** `senior-engineer.py`：post = "you are a senior engineer"，用 `subprocess.run(["python","gemini-2.5-flash.py", post+"\n"+input])` 呼叫 LLM 入口。
- **輸出解析包** `arrange-output.py`：`json.loads(stdin)` 後 `print(data["result"])`。
- **串接組合** `analysis_article.py`：內部依序 subprocess 呼 senior-engineer → arrange-output；或直接 shell pipe `cat article.txt | python senior-engineer.py | python arrange-output.py`。

## Session Library（usage.md 版）
回合制 RPG 模型：使用者行動 → 機器執行 → 等待下一輪。session 狀態主要在記憶體，其餘由 process 設計者決定。邊界：只負責 dict 讀寫與持久化，不處理 UI（slash command/args/file save UI 屬上層）。`Session` 類最簡實作（`get`/`set`，`set` 即寫回 JSON）；process 使用範例為每輪讀 history → append user → call_llm → append assistant → `session.set("history", ...)`。

## cli_lib / func_center（usage.md 版「之後」）
cli_lib：slash command（`/reset`/`/save`/`/history`）、args 支援、file save UI、big I/O control（file→stdin、stdout→file）。func_center：輕量 server 管理簡單 LLM+context 包，避免每次啟動 interpreter，`./func_center --func llm_summarize "input text"`。

## AI 驅動的自我擴展（usage.md 版）
使用者說「幫我做一個能做 XXX 的 process」→ AI ① 理解意圖 ② 實作符合約定的 process ③ 放進 processes 目錄 ④ `hub --build-list` 更新索引 ⑤ 從此可被呼叫/組合。「系統不是人工填充，而是透過 AI 不斷生長，每個新 process 都成為之後可組合的積木。」

---

# 四、architecture.md（舊版，結構視角）

## 元件總覽
所有元件（processes / hub / session_lib）都坐落在 OS（executable, CLI args, stdin/stdout, filesystem, pipe）之上，彼此獨立，透過 OS 機制與文件互動；共用一個 `list.json`（索引文件）。

## 元件職責表（architecture.md 獨有）
| 元件 | 形式 | 職責 | 不負責 |
|------|------|------|--------|
| **process** | executable file | 執行單一功能、輸出 metadata | 知道其他 process、管理會話 |
| **hub** | one-shot CLI tool | 掃描索引、查詢匹配 | 持續運行、執行 process |
| **session_lib** | Python library | dict 讀寫與持久化 | UI 邏輯、process 排程 |
| **list.json** | JSON file | 儲存所有 process 的 metadata 索引 | — |

## 四個資料流（architecture.md 獨有）
1. **建立索引**：`hub --build-list ./processes/` → 掃描 → 對每個檔 `subprocess(file --metadata)` → 彙整 → `list.json.tmp` → rename → `list.json`。
2. **查詢與呼叫**：`hub --search-func "translate"` → 讀 list.json → 回傳路徑 → 使用者/AI `subprocess(./processes/translate "input")` → process 讀 stdin/args 執行寫 stdout。
3. **多輪 session**：回合 1 與回合 2 都透過 `session.get/set` 經 `session.json` 橋接（session.json 是回合之間的橋）。
4. **AI 自我擴展**：使用者意圖 → AI（search 確認不重複 → 寫檔 → chmod +x → build-list）→ 新 process 進系統 → 未來可被搜尋/呼叫/組合。

## 文件系統佈局（architecture.md 版）
```
ai_core/
├── hub
├── processes/   (gemini-2.5-flash.py / senior-engineer.py / arrange-output.py / ...)
├── list.json
├── session_lib/session.py
└── ~/.{process_name}/session.json   ← 各 process 的 session 文件
```
`processes/` 扁平，太多再分類。

## 元件邊界（architecture.md 詳述）
- **Process**：負責自己的功能邏輯、`--metadata`、stdin/args 解析、stdout、自己的 session；不負責知道其他 process、排程選擇呼叫誰、UI。例外：組合 process 用 subprocess 呼別人但仍透過 OS。
- **Hub**：負責掃描、收集 metadata、讀寫 list.json、查詢；不負責執行 process、管 session、持續運行。
- **Session Library**：負責 dict 讀寫與 JSON 持久化；不負責 UI 邏輯、多 process 共享協調、排程。

## Process 內部結構（兩入口共用一檔）
```python
def metadata(): return {...}
def run(input_text): return output_text
if __name__ == "__main__":
    if "--metadata" in sys.argv: print(json.dumps(metadata()))
    else:
        input_text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(input_text))
```
兩入口共用同檔是約定不是強制。

## 通信機制表（architecture.md 獨有）
| 場景 | 機制 |
|------|------|
| process ↔ process（一次性組合） | shell pipe 或 subprocess |
| process ↔ session 狀態 | session.json 文件 |
| hub ↔ process（取 metadata） | subprocess（呼叫 `--metadata`） |
| hub ↔ list.json | 文件讀寫（atomic rename） |
| 使用者/AI ↔ 任何元件 | CLI args + stdin/stdout |

**沒有**：socket、HTTP、IPC、shared memory、message queue。
> ⚠️ 此處與 old/2 的「四種通信方式（含 HTTP/IPC/文件）」自相矛盾——正是 process 世代對 old/ 世代的否定。現行則又把常駐 server 納回規劃（見 note_07 分歧表）。

## 五條關鍵不變式（architecture.md 獨有）
1. Process 必須支援 `--metadata`，否則無法被索引。
2. list.json 用 atomic rename 寫入，否則讀寫會撞。
3. Session 狀態屬於單一 process，多 process 不共享同一份 session。
4. Hub 不持有任何狀態，重跑 `--build-list` 等同重置索引。
5. 每個 process 是黑盒，互不知曉、互不依賴。

## 與「更舊」架構的對應表（architecture.md 自記，指向 old/ 世代）
| 舊（已棄用，指 old/） | 新（指 process 世代） |
|--------------|-----|
| BaseFunction class | executable file |
| FunctionRegistry class | filesystem + list.json |
| Closure 物件 | process 內部變數 |
| Context 物件 | session.json |
| Server / REST API | hub one-shot + stdin/stdout |
| Layer 1 / 2 / 3 | process / hub / session_lib |
architecture.md 註：「舊架構的程式碼還在 `src/` 和 `old/`，但已不是當前設計。」（此「當前」指舊版內部的 process 世代，整體仍已被現行覆蓋。）

---

舊版的系統設計三世代見 [note_07_legacy_system_design.md](note_07_legacy_system_design.md)；舊版 process 清單與 agents 生態見 [note_09_legacy_processes_agents.md](note_09_legacy_processes_agents.md)。
