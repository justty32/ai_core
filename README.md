# ai_core

極簡的 AI 系統：把 OS 的 process 機制當成函數系統，由 AI 自我擴展。

```
process = 可執行文件 + --metadata 約定
hub     = 一次性的索引工具
session = 一個有持久化的 dict
AI      = 持續生成新 process 來擴展系統
```

設計理念見 [`SYSTEM_DESIGN.md`](SYSTEM_DESIGN.md)；架構視角見 [`architecture.md`](architecture.md)；使用層細節見 [`usage.md`](usage.md)。

---

## Setup

需要 Python 3.10+（測試在 3.12）。

```bash
python -m venv .venv
.venv/bin/pip install -r requirements.txt
```

**重要：所有命令都用 `.venv/bin/python` 跑**，hub 會把 `sys.executable` 傳給子 process，這樣 process 才能拿到 venv 裡的依賴（`litellm` 等）。

如果用 LLM 相關 process，請設定 API key，例如：

```bash
export GEMINI_API_KEY=...           # 預設 model: gemini/gemini-2.5-flash
# 或 LLM_MODEL=openai/gpt-4o-mini  OPENAI_API_KEY=...
```

---

## 快速試用

```bash
# 1. 建立索引
.venv/bin/python hub --build-list ./processes/

# 2. 搜尋
.venv/bin/python hub --search-func "translate"

# 3. 跑一個 process
echo "Hello, World!" | .venv/bin/python processes/slugify.py
# → hello-world

# 4. 跑測試
.venv/bin/python -m pytest tests/ -q
```

---

## Process 約定

每個 process 是一個可執行文件，必須支援兩種呼叫：

```bash
./my_process --metadata          # 輸出單行 JSON
./my_process "input"             # 正常執行（或從 stdin 讀）
```

### `--metadata` 必填 / 選填

| 欄位 | 必填 | 說明 |
|------|------|------|
| `name` | ✅ | process 名稱（與檔名一致） |
| `description` | ✅ | 一句話描述功能 |
| `version` | | 預設無 |
| `tags` | | 字串陣列，幫助搜尋 |
| `input` | | 輸入型態描述 |
| `output` | | 輸出型態描述 |
| `examples` | | `[{"input": "...", "output": "..."}]`；`hub --validate` 會逐一執行並比對輸出 |
| `io` | | `{"input": stdin\|file\|args\|none, "output": stdout\|file\|none, "format": {"input": text\|json\|binary, "output": ...}}`；給 orchestrator/export 用 |

### 錯誤與 I/O 契約

- **成功**：exit 0，結果寫 stdout。
- **失敗**：non-zero exit，錯誤訊息寫 stderr。
- **小輸入**：CLI args。
- **大/多行輸入**：stdin。**Process 必須兩種都能讀**：
  ```python
  text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
  ```

### 最小骨架

```python
#!/usr/bin/env python3
import sys, json

METADATA = {
    "name": "my_process",
    "description": "What it does in one line",
    "version": "1.0",
    "tags": ["..."],
    "input": "text",
    "output": "text",
    "examples": [
        {"input": "hi", "output": "HI"},
    ],
}

def run(text: str) -> str:
    return text.upper()

if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
```

---

## AI 寫 process 的標準流程

當 AI（或人）要新增一個 process，照這六步：

```bash
# 1. 確認沒有重複
./hub --search-func "<keyword>"

# 2. 寫 processes/<name>.py（依上面骨架）

# 3. 設執行權限
chmod +x processes/<name>.py

# 4. 驗證 metadata 契約 + 跑 examples
.venv/bin/python hub --validate processes/<name>.py

# 5. （可選）手動煙霧測試
echo "..." | .venv/bin/python processes/<name>.py

# 6. 重建索引
.venv/bin/python hub --build-list ./processes/
```

`hub --validate` 會檢查：執行權限、`--metadata` 能跑、輸出是合法 JSON 物件、含必填欄位、退出碼 0。
若 metadata 含 `examples`，會逐一把 `input` 餵進 stdin、比對 `output`——examples 是自動煙霧測試。

---

## Hub 命令

```bash
hub --build-list <dir> [--output <file>]    # 掃描目錄建索引
hub --search-func <query> [--list <file>]   # 子字串搜尋 (name/description/tags)
hub --validate <process_path>               # 檢查契約 + 跑 examples（若有）
hub --gen-agents-md [--output <file>]       # 產出給 AI 讀的入口文件
hub --gen-functions-md [--output <file>]    # 產出完整 process 清單
hub --export <fmt> [--output <file>]        # 輸出 tool schemas
                                            #   fmt: openai-tools | anthropic-tools
```

預設 `<file>` 是 `./list.json`，預設 `<dir>` 是 `./processes`。
所有 `--gen-*` / `--export` 在沒給 `--output` 時直接印到 stdout（pipe-friendly）。

**全域旗標**：`--json-errors` 把 stderr 錯誤改成單行 JSON
（`{"type", "message", "hint", "retriable"}`），給 orchestrator（planner/chain）程式化處理用。

**list.json 條目格式**：每個 entry 含 `_status: "ok|partial|absent"`。
`partial` 表示 metadata 解析了但缺必填欄位；`absent` 表示 `--metadata` 整個失敗。
search-func 會跳過 `absent` 但仍會返回 `partial`（讓 caller 自己決定）。

---

## 編排：planner + chain

兩個 meta-process，讓 AI 把複雜任務拆成 process 序列：

```bash
# planner：把自然語言任務轉成 JSON 執行計劃
.venv/bin/python processes/planner.py "review my Go code and write release notes"

# chain：吃 planner 的 JSON，依序執行每一步
.venv/bin/python processes/planner.py "..." | \
  .venv/bin/python processes/chain.py --input my_code.go
```

每一步的中間結果會存到 `~/.ai_core/chain/session.json`。

---

## 目錄

```
ai_core/
├── hub                 ← 索引工具（可執行）
├── list.json           ← hub --build-list 的輸出
├── processes/          ← 所有 process（扁平、44 個）
├── auto/               ← 自動產出：AGENTS.md / FUNCTIONS.md / tools.*.json
├── session_lib/        ← Session library 原始碼
├── tests/              ← pytest 測試
├── SYSTEM_DESIGN.md    ← 主要設計文檔
├── SYSTEM_DESIGN_EXTRA.md  ← 設計總結 + 已決策清單
├── architecture.md     ← 架構視角
├── usage.md            ← 使用者視角
└── CLAUDE.md           ← 給 AI 的專案指引
```
