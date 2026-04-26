# ai_core — Usage Design

## 核心概念

所有功能都是 **process**，process 就是一個可執行文件（shell script / .py / binary）。

```
process = 可執行文件 + CLI args
```

---

## Process 約定

每個 process 必須支援兩種呼叫方式：

```bash
./my_process --metadata          # 輸出自己的 metadata JSON
./my_process "input"             # 正常執行
```

### --metadata 格式

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

### I/O 約定

```bash
./process "arg"                  # 小輸入：CLI arg
./process < input.txt            # 大輸入：file → stdin
./process > output.txt           # 大輸出：stdout → file
./process < input.txt > output.txt
```

---

## Process 範例

### LLM 入口

```python
# gemini-2.5-flash.py
import sys, litellm

model = litellm.completion(model="gemini/gemini-2.5-flash", temperature=1.2)
input_text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
result = model(input_text)
print(result)
```

### Context 綁定包

```python
# senior-engineer.py
import subprocess, sys

post = "you are a senior engineer"
input_text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
result = subprocess.run(
    ["python", "gemini-2.5-flash.py", post + "\n" + input_text],
    capture_output=True, text=True
)
print(result.stdout)
```

### 輸出解析包

```python
# arrange-output.py
import sys, json

input_text = sys.stdin.read()
data = json.loads(input_text)
print(data["result"])
```

### 串接組合

```python
# analysis_article.py
import subprocess, sys

input_text = sys.stdin.read()

p1 = subprocess.run(["python", "senior-engineer.py"], input=input_text, capture_output=True, text=True)
p2 = subprocess.run(["python", "arrange-output.py"], input=p1.stdout, capture_output=True, text=True)
print(p2.stdout)
```

或直接用 shell pipe：

```bash
cat article.txt | python senior-engineer.py | python arrange-output.py
```

---

## Hub

hub 是一次性工具，不是 server，管理所有 process 的索引。

```bash
./hub --build-list ./processes/   # 掃描目錄，呼叫每個 --metadata，寫入 list.json
./hub --search-func "translate"   # 讀 list.json，回傳匹配的 process
```

**（未來功能）** `--search-func` 目前做精確/關鍵字匹配。未來可加入模糊搜尋，透過 function 的 description 或其他 metadata 做語意查詢，可用 LLM 或向量搜尋實現。

**list.json**（atomic write，先寫 tmp 再 rename）：

```json
[
  {"name": "translate", "path": "./processes/translate.py", "description": "..."},
  {"name": "summarize", "path": "./processes/summarize.sh", "description": "..."}
]
```

---

## Session Library

多輪 session 的運作模型像回合制 RPG：

```
使用者行動（輸入文字、按 Enter）
    → 機器開始執行（呼叫 process、LLM、運算）
    → 機器完成，等待下一輪使用者行動
    → 重複
```

每一輪結束後，狀態需要被保存，下一輪才能接續。session 狀態主要存在記憶體中，其餘由 process 設計者自行決定如何管理。session library 就是這個狀態的存取層，本質是一個有持久化的 dict。

session library 的邊界：只負責 dict 的讀寫與持久化，不處理使用者介面邏輯。slash command、args 支援、file save UI 等屬於上層，由 process 自己或未來的獨立 library 處理。

```python
# session.py
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

process 使用：

```python
from session import Session

session = Session("~/.my_process/session.json")

# 每一輪開始：讀取上一輪的狀態
history = session.get("history", [])

# 執行這一輪的邏輯
history.append({"role": "user", "content": input_text})
response = call_llm(history)
history.append({"role": "assistant", "content": response})

# 每一輪結束：保存狀態，等待下一輪
session.set("history", history)
print(response)
```

---

## cli_lib（之後）

建在 session library 之上的互動層 library，處理使用者介面相關的邏輯：

- **slash command** — `/reset`、`/save`、`/history` 等回合內指令
- **args 支援** — 從 CLI args 初始化 session、設定參數
- **file save UI** — 提示使用者儲存/載入 session 的介面流程
- **big I/O control** — 大輸入輸出的處理（file → stdin、stdout → file）

process 設計者可選擇性引入，不強制使用。

---

## func_center（之後）

輕量 server，管理簡單的 LLM+context 包，避免每次都啟動 Python interpreter。

```bash
./func_center --func llm_summarize "input text"
```

---

## AI 驅動的自我擴展

這是整個系統最核心的能力：**AI 來實作、擴展、填充這個系統**。

使用者說「幫我做一個能做 XXX 的 process」，AI：
1. 理解意圖
2. 實作一個符合約定的 process（支援 `--metadata`、stdin/stdout）
3. 放進 processes 目錄
4. 執行 `hub --build-list` 更新索引
5. 這個 process 從此可被呼叫和組合

系統不是由人工手動填充，而是透過 AI 不斷生長。每一個新 process 都成為之後可以被組合的積木。

---

## 架構總覽

```
[hub]                  ← 一次性，搜尋 list.json
    ↓
[processes]            ← 各種 shell command，支援 --metadata
    ↓
[session library]      ← 每個 process 自己用，管理回合狀態
    ↓
（之後）[func_center]  ← 輕量 LLM 包的 server
```
