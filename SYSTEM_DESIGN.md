---
name: System Design - Process-Based Architecture
description: ai_core 基於 process 的極簡架構，依託 OS 機制，由 AI 自我擴展
type: design
updated: 2026-04-26
---

# SYSTEM_DESIGN — Process-Based Architecture

ai_core 是一個極簡的 AI 系統，核心理念：

1. **所有功能都是 process**，依託 OS 機制（可執行文件 + CLI args + stdin/stdout）
2. **共同約定極小**，只有 `--metadata` 一條規則
3. **AI 自我擴展**，process 由 AI 實作和填充，不是人工建框架

---

## 核心理念

### 為什麼選 process？

之前的設計試圖建立 BaseFunction、FunctionRegistry、Client-Server 等抽象層，太複雜。

回歸最簡單的事實：**OS 已經有完美的 process 管理機制**。

- 可執行文件 = 函數定義
- CLI args = 函數參數
- stdin/stdout = I/O 通道
- 文件系統 = 函數註冊表
- pipe / subprocess = 函數組合

不需要重新發明這些。

### 唯一的共同約定

```bash
./any_process --metadata    # 輸出 JSON，描述自己
./any_process "input"       # 正常執行
```

像 `--help` / `--version` 一樣，是 CLI convention。任何 shell script、Python file、binary 都可以遵守。

---

## 系統元件

### 1. Process

最基本的單位。任何可執行文件，只要遵守 `--metadata` 約定。

**Metadata 格式：**

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

**I/O 約定：**

```bash
./process "arg"                  # 小輸入：CLI arg
./process < input.txt            # 大輸入：file → stdin
./process > output.txt           # 大輸出：stdout → file
./process < input.txt > output.txt
```

**Process 類型（依實作方式分）：**

- **LLM 入口** — 直接呼叫 LLM（如 `gemini-2.5-flash.py`）
- **Context 綁定包** — 預設 system prompt 後呼叫 LLM 入口（如 `senior-engineer.py`）
- **輸出解析包** — 解析或轉換另一個 process 的輸出（如 `arrange-output.py`）
- **組合 process** — 串接多個 process（內部 subprocess 呼叫，或讓使用者用 shell pipe）
- **任何其他** — shell script、純計算、外部工具的 wrapper 等

**組合方式：**

```bash
# Shell pipe（最自然）
cat article.txt | python senior-engineer.py | python arrange-output.py

# 在 process 內部用 subprocess
p1 = subprocess.run(["python", "senior-engineer.py"], input=text, ...)
p2 = subprocess.run(["python", "arrange-output.py"], input=p1.stdout, ...)
```

---

### 2. Hub

一次性工具，**不是 server**，管理所有 process 的索引。

```bash
./hub --build-list ./processes/   # 掃描目錄，呼叫每個 --metadata，寫入 list.json
./hub --search-func "translate"   # 讀 list.json，回傳匹配的 process
```

**list.json**（atomic write：先寫 tmp 再 rename，避免讀寫衝突）：

```json
[
  {"name": "translate", "path": "./processes/translate.py", "description": "..."},
  {"name": "summarize", "path": "./processes/summarize.sh", "description": "..."}
]
```

**為什麼不是 server？**

- 無狀態，沒有 socket、port、並發問題
- 失敗代價小（重跑就好）
- 文件系統就是天然的儲存

**未來功能：** `--search-func` 目前做精確/關鍵字匹配。未來可加入模糊搜尋（語意查詢），透過 LLM 或向量搜尋實現。

---

### 3. Session Library

幫助 process 管理多輪 session 狀態。

**運作模型（像回合制 RPG）：**

```
使用者行動（輸入文字、按 Enter）
    → 機器開始執行（呼叫 process、LLM、運算）
    → 機器完成，等待下一輪使用者行動
    → 重複
```

每一輪結束後狀態被保存，下一輪可接續。

**設計邊界：**

- ✅ 只負責 dict 的讀寫與持久化
- ❌ 不處理使用者介面邏輯（slash command、args、file save UI 都屬於上層）

**狀態存放：**

- 主要在記憶體
- 持久化由 process 設計者自行決定（多久存一次、存哪裡）

**最簡實作：**

```python
import json, os

class Session:
    def __init__(self, path="session.json"):
        self.path = path
        self.data = json.load(open(path)) if os.path.exists(path) else {}

    def get(self, key, default=None):
        return self.data.get(key, default)

    def set(self, key, value):
        self.data[key] = value
        json.dump(self.data, open(self.path, "w"))
```

**使用範例：**

```python
session = Session("~/.my_process/session.json")

# 每一輪開始：讀取上一輪狀態
history = session.get("history", [])

# 執行這一輪
history.append({"role": "user", "content": input_text})
response = call_llm(history)
history.append({"role": "assistant", "content": response})

# 每一輪結束：保存狀態
session.set("history", history)
print(response)
```

---

### 4. cli_lib（未來）

建在 session library 之上的互動層 library，處理使用者介面相關的邏輯：

- **slash command** — `/reset`、`/save`、`/history` 等回合內指令
- **args 支援** — 從 CLI args 初始化 session、設定參數
- **file save UI** — 提示使用者儲存/載入 session 的介面流程
- **big I/O control** — 大輸入輸出的處理流程

process 設計者可選擇性引入，不強制使用。

---

### 5. func_center（未來）

輕量 server，管理簡單的 LLM+context 包。

**動機：** 每個 process 都是文件，但啟動一個 Python process 有開銷（loading interpreter、imports 等）。對於頻繁呼叫的輕量函數（簡單 LLM 包），用 server 集中管理會更高效。

```bash
./func_center --func llm_summarize "input text"
```

只在需要時引入，初期所有 process 都是文件。

---

## AI 驅動的自我擴展

**這是整個系統最核心的能力。**

使用者說「幫我做一個能做 XXX 的 process」，AI：

1. 理解意圖
2. 實作一個符合約定的 process（支援 `--metadata`、stdin/stdout）
3. 放進 processes 目錄
4. 執行 `hub --build-list` 更新索引
5. 這個 process 從此可被呼叫和組合

**核心設計原則：**

> 每一個約定都要極度簡單，AI 才能輕易實作。
> 約定越簡單 → AI 實作門檻越低 → 系統擴展越快。

這就是為什麼整個系統只有 `--metadata` 一條規則：**讓 AI 寫 process 的負擔最小**。

系統不是人工填充的，而是透過 AI 自我生長。每個新 process 都成為之後可被組合的積木。

---

## 架構總覽

```
[hub]                    ← 一次性工具，搜尋 list.json
    ↓
[processes]              ← 各種 shell command，支援 --metadata
    │
    ├─ session library   ← 每個 process 自己用，管理回合狀態
    │
    └─ AI 不斷生成新 process

（未來）
[cli_lib]                ← 互動層，slash command、args、UI
[func_center]            ← 輕量 LLM 包的 server
```

---

## 設計原則

✅ **依託 OS** — 不重新發明 process 管理、文件系統、pipe
✅ **約定極簡** — 只有 `--metadata`，越簡單 AI 越容易實作
✅ **無狀態核心** — hub 一次性、不是 server，無並發問題
✅ **AI 自我擴展** — 系統由 AI 持續填充，不是人工建框架
✅ **回合制模型** — session 狀態以回合為單位管理
✅ **延遲優化** — func_center、cli_lib 都是「之後」，不是現在

---

## 與之前設計的差異

**棄用：**

- BaseFunction 抽象類別（直接用 OS 的 executable）
- FunctionRegistry 類（直接用文件系統 + list.json）
- Closure / Context 的形式區分（每個 process 自己決定）
- Client-Server 持續服務（hub 改為一次性）
- REST API / Python Module / Pipe 多種通信（只有 stdin/stdout）

**保留：**

- 函數組合的 LISP 思想（用 shell pipe / subprocess 體現）
- Metadata 的描述性設計（簡化欄位）
- 多客户端的分離（每個 process 獨立）

**新增：**

- AI 自我擴展作為核心驅動力
- 回合制 session 模型
- 約定簡化到極致的設計原則

---

## 下一步

1. 實作最簡 hub（`--build-list` + `--search-func`）
2. 寫一兩個範例 process（LLM 入口、context 綁定包）
3. 實作最簡 session library
4. 試著讓 AI 透過約定生成新的 process
5. 觀察哪裡需要 cli_lib 或 func_center
