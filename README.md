# ai_core

> 用 LISP 式的函數組合思維，重新架構 LLM 的呼叫與管理。

把 LLM 視為一個純函數 `tokens → tokens`，所有複雜行為都是這個基本函數的**組合與包裝**。沒有魔法，只有 closure 與 transform。

```python
agent = take(json_schema,
         bind(system_prompt,
         make_model("claude-sonnet-4-6", temperature=0.7)))

result = agent("使用者的輸入")
```

---

## 設計理念

**Core 是一個無狀態的函數執行服務**。它只做兩件事：

1. **管理函數**（註冊、查找、組合、生命週期）
2. **執行函數**（透過統一的 `(tokens, context) → (tokens, context)` 介面）

**會話、UI、平台特定邏輯都不在 Core 內。** 客户端（CLI、桌面、網頁、行動裝置）各自管理自己的 context，透過 REST API、Python module 或其他方式與 Core 通信。

---

## 三層架構

| 層 | 責任 | 主要組件 |
|---|---|---|
| **L1 函數定義** | 定義一個函數 | `BaseFunction`、`LLMFunction`、`ShellFunction`、`CalculateFunction`、`CompositeFunction` |
| **L2 函數管理** | 管理已註冊的函數 | `FunctionRegistry`（含索引、依賴追蹤） |
| **L3 客户端介面** | 對外通信 | `CoreService`、FastAPI、`HTTPClient` |

每個函數有三個組成部分：
- **Closure** — 函數自身的數據（建構時固定）
- **Context** — 會話級的全局數據（呼叫者擁有，可變）
- **Metadata** — 函數的描述資訊（標籤、資源特性等）

詳見 `SYSTEM_DESIGN.md` 與 `ARCHITECTURE.md`。

---

## 專案狀態

🚧 **骨架階段**

- ✅ 系統設計完成
- ✅ 架構規劃完成
- ✅ 程式碼骨架（含 type hints、docstring、測試）
- ✅ 詳細的實作計劃（`IMPLEMENTATION_PLAN.md`）
- ⏳ 實作（等待中）

骨架中所有方法主體都是 `raise NotImplementedError`，但測試已撰寫完成、行為定義明確，可以照著 `IMPLEMENTATION_PLAN.md` 的步驟逐項填入。

---

## 安裝

需要 Python 3.12+ 與 [uv](https://github.com/astral-sh/uv)：

```bash
git clone <repo-url>
cd ai_core
uv sync --extra dev
```

---

## 快速開始

### 1. 最簡使用（純 Python 模組）

```python
from ai_core import CoreService, CalculateFunction

core = CoreService()
core.register(CalculateFunction("upper_v1", lambda tokens: tokens.upper()))

output, context = core.execute("upper_v1", b"hello world", context={})
print(output.decode())  # → "HELLO WORLD"
```

### 2. 函數組合（S-expression 風格)

```python
from ai_core import CalculateFunction, CompositeFunction, CoreService

core = CoreService()
core.register(CalculateFunction("reverse_v1", lambda b: b[::-1]))
core.register(CalculateFunction("upper_v1", lambda b: b.upper()))

# 把多個函數串成一個 pipeline
pipe = CompositeFunction(
    "pipe_v1",
    ["reverse_v1", "upper_v1"],
    registry=core.registry,
)
core.register(pipe)

output, _ = core.execute("pipe_v1", b"hello", context={})
print(output.decode())  # → "OLLEH"
```

### 3. 啟動 HTTP 服務

```bash
uv run python examples/03_run_server.py
```

服務會在 `http://localhost:8000` 啟動，附帶自動產生的 OpenAPI 文件（`/docs`）。

### 4. 使用互動式 CLI

```bash
# 一個終端機開服務
uv run python examples/03_run_server.py

# 另一個終端機開 CLI
uv run python examples/05_cli.py
```

CLI 指令：

```
/functions          列出所有函數
/use <id>           設定預設函數
/context            顯示當前 context
/clear              清空 context
/save /load <path>  存取 context 到檔案
/help               顯示說明
/exit               離開
```

---

## 專案結構

```
ai_core/
├── README.md                 ← 本檔
├── CLAUDE.md                 ← 給 Claude Code 的專案指引
├── SYSTEM_DESIGN.md          ← 三層架構的設計理念（主要）
├── SYSTEM_DESIGN_EXTRA.md    ← 設計總結
├── ARCHITECTURE.md           ← 具體建構規格（型別、介面、資料流）
├── IMPLEMENTATION_PLAN.md    ← 給實作者的逐步任務清單
├── for_agent.md              ← 給 AI agent 的專案 onboarding
├── pyproject.toml            ← uv 專案設定
├── src/ai_core/
│   ├── functions/            ← L1 — 函數定義
│   ├── registry/             ← L2 — 函數註冊表
│   ├── service/              ← L3 — Core service、API、HTTPClient
│   └── persistence/          ← Core 儲存/載入
├── tests/                    ← pytest 測試
├── examples/                 ← 5 個可執行範例
│   ├── 01_basic_usage.py
│   ├── 02_composite.py
│   ├── 03_run_server.py
│   ├── 04_save_load_core.py
│   └── 05_cli.py
├── scripts/shell/            ← 範例 shell function
└── old/                      ← 之前的設計迭代（僅供參考）
```

---

## 測試

```bash
uv run pytest -v
```

所有測試都針對骨架的預期行為而寫——實作完成後，這些測試應該全部通過。

---

## 文件閱讀順序

依照角色不同：

- **想理解設計理念** → `SYSTEM_DESIGN.md`
- **要實作或修改程式** → `ARCHITECTURE.md` → `IMPLEMENTATION_PLAN.md`
- **是 AI agent 接手** → `for_agent.md`
- **只是想用一下** → 本檔的「快速開始」就夠了

---

## 設計原則

✅ **LISP 風格** — 函數是一等公民，可組合、可引用
✅ **極簡核心** — Core 無狀態，只做函數管理與執行
✅ **元數據驅動** — 所有決策基於 metadata
✅ **客户端自主** — 每個會話獨立管理自己的 context
✅ **多種通信方式** — REST API、Python module、System pipe
✅ **函數獨立** — 每個函數是黑盒，易於版本管理與替換

---

## 授權

MIT

---

## 作者

Justty (justty32@gmail.com)
