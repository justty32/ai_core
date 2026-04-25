---
name: System Design Layer 1 - Advanced Concepts
description: 函數組合、調度、實例化等高層設計概念
type: project
---

# System Design 1 — Advanced Concepts

## 概述

本文檔記錄在 SYSTEM_DESIGN.md 基礎上進行的深入腦力激盪，涉及：
- S-expression 風格的函數組合
- 調度和資源管理
- 函數定義與實例化的分離
- 利用操作系統機制

**注意：這是衍生層。更基礎的概念應該在 system_design_2.md 中。**

---

## 1. S-Expression 風格的函數組合（Chained Calling）

### 概念

不同於線性的管道（`func1 → func2 → func3`），系統應該支持樹狀或圖狀的函數組合，採用 S-expression 的形式。

### 表示方式

#### 形式 1：LISP 風格

```lisp
(call_llm 
  (contextualize 
    (denoise_voice tokens metadata)
    session_id)
  user_profile)
```

#### 形式 2：嵌套字典（Python 表示）

```python
composition = {
    "func": "call_llm",
    "args": [
        {
            "func": "contextualize",
            "args": [
                {
                    "func": "denoise_voice",
                    "args": ["$tokens", "$metadata"]
                },
                "$session_id"
            ]
        },
        "$user_profile"
    ]
}
```

### 特點

1. **多輸入函數** — 函數可以接收多個來源的輸入（不只是前一個函數的輸出）
2. **樹狀執行** — 執行順序由依賴關係決定，而非固定順序
3. **並行可能性** — 獨立的分支可以並行執行

### 待決策問題

1. 如何動態評估這個樹狀結構？（自下而上？自上而下？）
2. 變數解析：`$tokens`, `$metadata` 等來自哪裡？
3. 樹中哪些部分可以並行？
4. 執行失敗時如何恢復？

---

## 2. 調度和資源管理（OS-Style Scheduling）

### 背景

LLM 處理可能花費大量時間（數秒到數十秒）。在這段時間內，系統不應該阻塞其他會話的執行。

### 概念

系統應該像**操作系統的進程調度器**一樣工作：

```
時間軸：

0ms   ├─ Session A: denoise_voice() [0-100ms]
      ├─ Session B: denoise_voice() [0-100ms]
      └─ Session C: denoise_voice() [0-100ms]

100ms ├─ Session A: call_llm() [開始，預計 5000ms]  ⏳
       ├─ Session B: [暫停，等待 LLM 配額]
       └─ Session C: [轉到不依賴 LLM 的後續步驟] 👷
```

### 任務狀態機

```
pending ──→ running ──→ waiting ──→ completed
            │                        ↑
            └────→ suspended ────────┘
```

**狀態說明：**
- `pending` — 等待調度執行
- `running` — 正在 CPU 上執行
- `waiting` — 等待某個資源（如 LLM API 響應）
- `suspended` — 被調度器暫停，讓出 CPU
- `completed` — 執行完成

### 時間統計的用途

函數元數據中的時間統計用來做**預測性調度**：

```python
# 示例：denoise_voice 預計 100ms，call_llm 預計 5000ms

if function.resource_profile.time.actual_work < THRESHOLD:
    # 短時間操作，立即執行
    execute_now(function)
else:
    # 長時間操作（如 LLM 調用）
    # 檢查資源配額和優先級
    schedule_with_priority(function)
```

### 利用操作系統機制

**不要自己實現調度器，直接利用 OS 提供的工具：**

#### 方案 A：多進程（Multiprocessing）

```python
import multiprocessing

# 每個函數執行是一個 subprocess
process_a = multiprocessing.Process(target=call_llm, args=(tokens_a,))
process_b = multiprocessing.Process(target=call_llm, args=(tokens_b,))

# OS 自動調度這些進程
process_a.start()
process_b.start()

process_a.join()
process_b.join()
```

**優點：** 真正的並行（多核），隔離性好
**缺點：** 開銷大，進程間通信複雜

#### 方案 B：非同步 I/O（Asyncio）

```python
import asyncio

async def process_all():
    return await asyncio.gather(
        call_llm_async(tokens_a),
        call_llm_async(tokens_b),
        call_llm_async(tokens_c)
    )

# OS 事件循環自動管理上下文切換
asyncio.run(process_all())
```

**優點：** 輕量，自動調度，易於實現
**缺點：** 單核執行，需要函數都支持 async

#### 方案 C：Coroutine（最輕量）

```python
# 使用語言級的 coroutine
# 執行模型完全由運行時決定
```

**優點：** 最簡潔，語言級支持
**缺點：** 需要語言 runtime 的支持

### 資源限制的考慮

```python
# 例如：LLM API 同時只能處理 N 個請求
llm_semaphore = asyncio.Semaphore(N)

async def call_llm_with_limit(tokens):
    async with llm_semaphore:
        return await llm_api.call(tokens)
```

---

## 3. 函數定義 vs 實例化分離

### 概念

採用編譯/運行時的分離模型：

- **Definition Time（定義時期）** — 函數類的定義，包含邏輯和元數據
- **Instantiation Time（實例化時期）** — 為每次執行創建新的實例（類似 OS 創建進程）
- **Runtime（執行時期）** — 實例執行邏輯
- **Cleanup（清理時期）** — 釋放資源

### 詳細流程

```python
# ═══════════════════════════════════
# Definition Time：定義函數類
# ═══════════════════════════════════

class DenoiseVoiceFunction(BaseFunction):
    """函數定義（代碼）"""
    
    def __init__(self):
        self.id = "denoise_voice_v1"
        self.type = "preprocessing"
        self.resource_profile = {
            "memory": {"self": 5_000_000},
            "time": {"actual_work": 100}
        }
    
    def __call__(self, tokens, metadata):
        # 邏輯實現
        pass


# ═══════════════════════════════════
# Instantiation Time：為會話創建實例
# ═══════════════════════════════════

function_definitions = {
    "denoise_voice_v1": DenoiseVoiceFunction
}

# Session A 的實例
session_a_func_instance = function_definitions["denoise_voice_v1"]()
# 分配棧空間、初始化本地變數、設置執行上下文

# Session B 的實例（獨立的棧和本地狀態）
session_b_func_instance = function_definitions["denoise_voice_v1"]()


# ═══════════════════════════════════
# Runtime：執行實例
# ═══════════════════════════════════

result_a = session_a_func_instance(tokens_a, metadata_a)
result_b = session_b_func_instance(tokens_b, metadata_b)
# 兩個實例可以並行執行，各有自己的棧、本地變數
# 互不污染


# ═══════════════════════════════════
# Cleanup：清理資源
# ═══════════════════════════════════

# 實例執行完成後
del session_a_func_instance  # 釋放棧空間
del session_b_func_instance  # 釋放棧空間
```

### 優點

1. **重用性** — 同一個函數定義被多個會話共用（節省記憶體）
2. **隔離性** — Session A 的棧不會污染 Session B（安全性）
3. **資源管理** — 實例化時分配，清理時釋放（自動管理）
4. **真正的並行** — 多個實例同時執行，互不干擾（高效）
5. **狀態追蹤** — 每個實例知道自己的執行狀態和進度（可觀測性）

### 與 Layer 1 的影響

這改變了 `BaseFunction` 的設計哲學：

```python
# 之前：函數本身就是可執行的
func = DenoiseVoiceFunction()
result = func(tokens, metadata)  # 直接執行

# 現在：定義和實例是分離的
FunctionClass = function_registry["denoise_voice_v1"]
func_instance = FunctionClass()  # 實例化
result = func_instance(tokens, metadata)  # 執行實例
```

---

## 4. 利用操作系統系統函數

### 核心想法

不要自己實現複雜的調度、上下文切換、資源管理等。直接利用**操作系統已經做好的機制**。

### 可利用的 OS 機制

| OS 機制 | Python 實現 | 用途 |
|--------|----------|------|
| 進程調度 | `multiprocessing` | 真正的並行執行 |
| 事件循環 | `asyncio` | 輕量任務調度 |
| Coroutine | `async/await` | 語言級任務管理 |
| 信號量 | `threading.Semaphore` | 資源限制 |
| 互斥鎖 | `threading.Lock` | 同步訪問 |
| 管道 | `os.pipe()` | 進程間通信 |
| 共享記憶體 | `multiprocessing.SharedMemory` | 進程間數據共享 |

### 設計原則

> **讓 OS 做它擅長的事，我們專注於業務邏輯。**

### 示例：使用 Asyncio

```python
import asyncio

class CoreEnvironment:
    def __init__(self):
        self.function_registry = {}
        self.session_contexts = {}
        self.execution_queue = asyncio.Queue()
    
    async def process_request(self, request):
        """非同步處理請求"""
        tokens = request["tokens"]
        metadata = request["metadata"]
        
        # 選擇函數鏈
        func_chain = self.select_function_chain(metadata)
        
        # 異步執行（讓 OS 事件循環調度）
        for func in func_chain:
            tokens, metadata = await self.execute_async(func, tokens, metadata)
        
        return {"tokens": tokens, "metadata": metadata}
    
    async def execute_async(self, func, tokens, metadata):
        """如果函數是長時間操作（如 LLM 調用），使用異步"""
        if func.resource_profile.time.actual_work > LONG_TASK_THRESHOLD:
            # 長時間操作，交給事件循環管理
            return await asyncio.to_thread(func, tokens, metadata)
        else:
            # 快速操作，直接執行
            return func(tokens, metadata)
```

---

## 5. 集成這些概念

### 完整流程圖

```
S-expression 組合
    ↓
評估樹狀結構
    ↓
生成任務圖（DAG）
    ↓
調度器決策
├─ 短時間任務 → 立即執行
├─ 長時間任務 → 檢查資源，條件執行
└─ 並行任務 → 使用 OS 機制並行
    ↓
實例化函數
    ↓
執行（同步/異步）
    ↓
狀態轉移（pending → running → waiting → completed）
    ↓
清理資源
```

### 核心問題仍需決策

1. **S-expression 評估** — 自下而上還是自上而下？
2. **變數解析** — 如何處理 `$tokens`, `$metadata` 等動態變數？
3. **並行粒度** — 多進程、多線程還是異步？
4. **資源限制** — 如何定義和強制資源配額？
5. **錯誤處理** — 中間函數失敗時的恢復策略？
6. **可觀測性** — 如何監控和調試複雜的執行圖？

---

## 待進一步討論

1. S-expression 的具體評估算法
2. 如何結合函數定義/實例化與元數據系統
3. 實際的調度決策算法
4. 多會話間的資源競爭管理
5. 系統整體的故障恢復機制

---

## 設計原則

1. **利用 OS 而非重發明輪子** — 調度、資源管理交給 OS
2. **定義/實例分離** — 效率和隔離的平衡
3. **樹狀組合** — 靈活的函數組織
4. **狀態機驅動** — 清晰的執行狀態
5. **動態評估** — 運行時根據資源決策
