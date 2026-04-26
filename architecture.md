# ai_core — Architecture

從結構視角看 ai_core：元件、資料流、邊界、職責。

設計理念見 `SYSTEM_DESIGN.md`，使用方式見 `usage.md`。

---

## 元件總覽

```
┌─────────────────────────────────────────────────────────┐
│                          OS                             │
│  (executable, CLI args, stdin/stdout, filesystem, pipe) │
└─────────────────────────────────────────────────────────┘
            ↑                 ↑                 ↑
            │                 │                 │
    ┌───────┴───────┐  ┌──────┴──────┐  ┌───────┴───────┐
    │   processes   │  │     hub     │  │ session_lib   │
    │               │  │             │  │               │
    │ shell / .py   │  │  one-shot   │  │  dict + 持久化 │
    │ binary / ...  │  │  index tool │  │               │
    └───────┬───────┘  └──────┬──────┘  └───────┬───────┘
            │                 │                 │
            └─────────────────┼─────────────────┘
                              │
                       ┌──────┴──────┐
                       │  list.json  │
                       │  (索引文件)  │
                       └─────────────┘
```

每個元件都是獨立的，透過 OS 機制和文件互動。

---

## 元件職責

| 元件 | 形式 | 職責 | 不負責 |
|------|------|------|--------|
| **process** | executable file | 執行單一功能、輸出 metadata | 知道其他 process、管理會話 |
| **hub** | one-shot CLI tool | 掃描索引、查詢匹配 | 持續運行、執行 process |
| **session_lib** | Python library | dict 讀寫與持久化 | UI 邏輯、process 排程 |
| **list.json** | JSON file | 儲存所有 process 的 metadata 索引 | — |

---

## 資料流

### 流程 1：建立索引

```
[使用者 / AI]
     │
     │  ./hub --build-list ./processes/
     ↓
   [hub] ──► 掃描 ./processes/*
     │
     │  for each: subprocess(file --metadata)
     ↓
   [process_1 --metadata]  →  metadata JSON
   [process_2 --metadata]  →  metadata JSON
   [process_3 --metadata]  →  metadata JSON
     │
     │  彙整
     ↓
   list.json.tmp  →  rename  →  list.json
```

### 流程 2：查詢與呼叫

```
[使用者 / AI]
     │
     │  ./hub --search-func "translate"
     ↓
   [hub] ──► 讀 list.json
     │
     │  匹配 → 回傳 process 路徑
     ↓
   [使用者 / AI]
     │
     │  subprocess(./processes/translate "input")
     ↓
   [translate process]
     │
     │  讀 stdin / args，執行，寫 stdout
     ↓
   [結果]
```

### 流程 3：多輪 session

```
回合 1                       回合 2
[使用者輸入]                 [使用者輸入]
     │                            │
     ↓                            ↓
[process]                    [process]
     │                            │
     │ session.get("history")     │ session.get("history")
     ↓                            ↓
[session.json]  ◄───────────  [session.json]
     │                            │
     │ session.set("history",...) │ session.set("history",...)
     ↓                            ↓
[寫入]                       [寫入]
     │                            │
     ↓                            ↓
[輸出]                       [輸出]
```

session.json 是回合之間的橋。

### 流程 4：AI 自我擴展

```
[使用者意圖]
   "幫我做一個能 XXX 的 process"
        │
        ↓
   [AI]
        │
        ├─► ./hub --search-func "..."     # 確認沒有重複
        │
        ├─► 寫 process 文件                # 實作 + --metadata
        │
        ├─► chmod +x ./processes/new_one   # 可執行
        │
        ├─► ./hub --build-list             # 更新索引
        │
        ↓
   [新 process 進入系統]
        │
        ↓
   未來可被搜尋、呼叫、組合
```

---

## 文件系統佈局

```
ai_core/
├── hub                          ← hub 可執行檔
├── processes/                   ← 所有 process 放這裡
│   ├── gemini-2.5-flash.py
│   ├── senior-engineer.py
│   ├── arrange-output.py
│   └── ...
├── list.json                    ← hub --build-list 的輸出
├── session_lib/                 ← session library 原始碼
│   └── session.py
└── ~/.{process_name}/           ← 各 process 的 session 文件
    └── session.json
```

`processes/` 是扁平目錄，未來如果太多再考慮分類。

---

## 元件邊界

### Process 的邊界

**負責：**
- 自己的功能邏輯
- `--metadata` 輸出
- stdin / args 的解析
- stdout 輸出
- 自己的 session（如果需要）

**不負責：**
- 知道其他 process 的存在
- 排程或選擇要呼叫誰
- UI 互動

**例外：** 組合 process 會用 subprocess 呼叫其他 process，但仍透過 OS 機制，不直接耦合。

---

### Hub 的邊界

**負責：**
- 掃描 processes/ 目錄
- 呼叫 `--metadata` 收集資訊
- 寫入 / 讀取 list.json
- 查詢匹配

**不負責：**
- 執行 process
- 管理 session
- 持續運行（一次性工具）

---

### Session Library 的邊界

**負責：**
- dict 的讀寫
- 文件持久化（JSON）

**不負責：**
- UI 邏輯（slash command、args 解析）
- 多 process 共享協調
- 排程

---

## Process 內部結構

每個 process 都有兩個入口（共用同一個文件）：

```python
# my_process.py

def metadata():
    return {
        "name": "my_process",
        "description": "...",
        ...
    }

def run(input_text):
    # 主邏輯
    return output_text

if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(metadata()))
    else:
        input_text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(input_text))
```

兩個入口共用同一個文件是約定，不是強制。process 可以是任何形式，只要呼叫 `--metadata` 時輸出正確 JSON、其他時候做正確的事即可。

---

## 通信機制

| 場景 | 機制 |
|------|------|
| process ↔ process（一次性組合） | shell pipe 或 subprocess |
| process ↔ session 狀態 | session.json 文件 |
| hub ↔ process（取 metadata） | subprocess（呼叫 `--metadata`） |
| hub ↔ list.json | 文件讀寫（atomic rename） |
| 使用者 / AI ↔ 任何元件 | CLI args + stdin/stdout |

**沒有：** socket、HTTP、IPC、shared memory、message queue。

---

## 未來元件的位置

```
                         (現在)
[processes] ─ [hub] ─ [session_lib]
       │
       │  (未來，按需引入)
       ↓
   ┌──────────────────────┐
   │  cli_lib              │
   │  - slash command      │
   │  - args 支援          │
   │  - file save UI       │
   │  - big I/O control    │
   └──────────────────────┘

   ┌──────────────────────┐
   │  func_center          │
   │  - 輕量 LLM 包 server  │
   │  - 避免 interpreter    │
   │    啟動開銷            │
   └──────────────────────┘
```

引入順序：先讓核心三元件能跑、AI 能寫 process，觀察痛點再決定何時加。

---

## 關鍵不變式

1. **Process 必須支援 `--metadata`** — 否則無法被索引
2. **list.json 用 atomic rename 寫入** — 否則讀寫會撞
3. **Session 狀態屬於單一 process** — 多 process 不共享同一份 session
4. **Hub 不持有任何狀態** — 重跑 `--build-list` 等同重置索引
5. **每個 process 是黑盒** — 互不知曉、互不依賴

---

## 與舊架構的對應

| 舊（已棄用） | 新 |
|--------------|-----|
| BaseFunction class | executable file |
| FunctionRegistry class | filesystem + list.json |
| Closure 物件 | process 內部變數 |
| Context 物件 | session.json |
| Server / REST API | hub one-shot + stdin/stdout |
| Layer 1 / 2 / 3 | process / hub / session_lib |

舊架構的程式碼還在 `src/` 和 `old/`，但已不是當前設計。
