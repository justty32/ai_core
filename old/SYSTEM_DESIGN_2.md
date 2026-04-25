---
name: System Design 2 - Complete Core Architecture
description: ai_core 完整系統設計，從願景到實現，包含所有細節
type: project
updated: 2026-04-25
---

# SYSTEM_DESIGN_2 — Complete Core Architecture & Implementation

**核心概念：** 用 LISP 式函數式思維架構 LLM，構建一個極簡、松耦合、無限可擴展的系統。

---

## 目錄

1. [願景 & 核心理念](#願景--核心理念)
2. [極簡架構](#極簡架構)
3. [核心的四大職責](#核心的四大職責)
4. [函數的四種類型](#函數的四種類型)
5. [通信方式 & OS 層](#通信方式--os-層)
6. [多客戶端架構](#多客戶端架構)
7. [Layer 1 數據結構](#layer-1-數據結構)
8. [Layer 1 函數定義](#layer-1-函數定義)
9. [函數元數據與自動組合](#函數元數據與自動組合)
10. [執行模型 & 調度](#執行模型--調度)
11. [會話管理](#會話管理)
12. [實現優先級](#實現優先級)
13. [設計原則](#設計原則)

---

## 願景 & 核心理念

### 基本哲學

把 **LLM 視為一個純函數**：

```
llm : tokens → tokens
```

所有複雜行為都是這個基本函數的組合與包裝，沒有魔法，只有 **closure 與 transform**。

### 三層核心抽象

#### Layer 0 — `make_model`：配置模型

```python
def make_model(model, temperature, top_k, ...):
    def call(tokens):
        return call_llm(model, temperature, top_k, tokens)
    return call
```

把模型名稱與參數封裝進 closure，回傳一個純粹的 `tokens → tokens` 函數。

#### Layer 0 — `bind`：綁定 context（偏函數應用）

```python
def bind(context, model_fn):
    def call(input_tokens):
        return model_fn(context + input_tokens)
    return call
```

把 system prompt / few-shot examples 等 context 預先綁入函數，產生一個新的、更專化的函數。

#### Layer 0 — `take`：解析輸出

```python
def take(schema, fn):
    def call(input_tokens):
        raw = fn(input_tokens)
        return parse(schema, raw)
    return call
```

把 LLM 的原始 token 輸出轉換為結構化資料。

### 組合範例

```python
agent = take(json_schema,
         bind(system_prompt,
         make_model("claude-sonnet-4-6", temperature=0.7, top_k=40)))

result = agent("使用者的輸入")
```

每一層都是對前一層的包裝，整體就是函數組合。

---

## 極簡架構

### 關鍵轉變

**不要把工具、框架、Agent 等納入核心，而是把它們看作外部的「左右臂」，通過標準 OS 層通信。**

```
之前（複雜）：
┌─────────────────────────────────────────┐
│ Core (包含所有邏輯)                     │
│ ├─ 調度                                  │
│ ├─ LLM 集成                             │
│ ├─ 框架集成 (LangChain, DSPy)          │
│ ├─ Tool 調用                            │
│ ├─ 成本統計                             │
│ └─ 資源管理                             │
└─────────────────────────────────────────┘

現在（極簡）：
┌─────────────────────────────────────┐
│ Core (只做必要的事)                  │
│ ├─ Task Router                       │
│ ├─ Session Manager                   │
│ ├─ Function Registry                 │
│ └─ LiteLLM Client（唯一框架依賴）   │
└─────────────────────────────────────┘
         │ (OS Layer)
  ┌──────┼────────────────────┐
  ▼      ▼                     ▼
Shell  External API          LiteLLM
Cmds   (LangChain,            (+ models)
       DSPy, etc.)
```

### 核心優勢

✅ **極簡** — 核心代碼 ~200 行（包括註釋）
✅ **單一依賴** — 只依賴 LiteLLM
✅ **松耦合** — 所有工具通過標準 OS 層通信
✅ **易擴展** — 新工具只需新增配置
✅ **成本透明** — 用戶完全掌控成本
✅ **語言無關** — 任何語言的工具都可集成
✅ **輕量部署** — 核心可獨立部署

---

## 核心的四大職責

### 職責 1：Task Router

```python
def determine_function(tokens: bytes, metadata: dict) -> str:
    """
    根據 tokens 和 metadata 決定調用哪個函數
    
    這是唯一的智能決策點
    可以使用 LiteLLM 進行簡單的分類
    """
    
    # 簡單啟發式規則或 LLM 分類
    text = tokens.decode().lower()
    
    if "optimize" in text:
        return "dspy_optimize"
    elif "agent" in text:
        return "langchain_agent"
    else:
        return "default_llm"
```

### 職責 2：Session Manager

```python
class SessionManager:
    """管理會話的生命週期和上下文"""
    
    def __init__(self):
        self.sessions = {}  # session_id → context
    
    def get_or_create_session(self, session_id: str):
        """獲取或創建會話"""
        if session_id not in self.sessions:
            self.sessions[session_id] = {
                "created_at": time.time(),
                "context": [],
                "metadata": {}
            }
        return self.sessions[session_id]
    
    def cleanup_session(self, session_id: str):
        """清理會話（由客戶端通過 session_end 標記觸發）"""
        if session_id in self.sessions:
            del self.sessions[session_id]
```

### 職責 3：Function Registry

```python
class FunctionRegistry:
    """註冊和管理函數"""
    
    def __init__(self):
        self.functions = {}
    
    def register(self, func_id: str, func_config: dict):
        """
        註冊一個函數（可以是任何類型）
        
        config 格式：
        {
            "type": "shell" | "python" | "api" | "llm",
            "path": "...",          # for shell
            "endpoint": "...",      # for api
            "model": "...",         # for llm
            "instance": ...         # for python
        }
        """
        self.functions[func_id] = func_config
```

### 職責 4：LiteLLM Client（唯一框架依賴）

```python
class LiteLLMClient:
    """
    調用任何 LLM 提供商
    這是核心唯一的框架依賴
    """
    
    def __init__(self):
        self.client = litellm.completion
    
    def call(self, model: str, messages: list, **kwargs):
        """統一的 LLM 調用接口"""
        return self.client(
            model=model,
            messages=messages,
            **kwargs
        )
```

---

## 函數的四種類型

### 類型 1：Shell 命令（外部進程）

```bash
#!/bin/bash
# 輸入：JSON 格式的 task（通過命令行參數）
TASK_JSON="$1"

# 解析並處理
TOKENS=$(echo "$TASK_JSON" | jq -r '.tokens')

# 執行邏輯
result=$(echo "$TOKENS" | some_processing)

# 輸出：JSON 格式（通過 stdout）
echo "{\"tokens\": \"$result\", \"metadata\": {}}"
```

**配置：**
```python
{
    "id": "shell_process_v1",
    "type": "shell",
    "path": "/path/to/process.sh"
}
```

**執行方式：**
```python
result = subprocess.run(
    ["bash", config["path"], json.dumps(task)],
    capture_output=True,
    timeout=timeout
)
```

### 類型 2：本地 Python 函數（進程內）

```python
from ai_core.base import BaseFunction

class CustomFunction(BaseFunction):
    def __init__(self):
        self.id = "custom_v1"
        self.type = "processing"
        self.resource_profile = {
            "memory": {"self": 0, "running": 0, "peak": 0},
            "time": {"startup": 0, "actual_work": 0, "teardown": 0}
        }
    
    def __call__(self, tokens: bytes, metadata: dict):
        # 實現邏輯
        result = self.process(tokens)
        return result, metadata
```

**配置：**
```python
{
    "id": "custom_v1",
    "type": "python",
    "instance": CustomFunction()
}
```

### 類型 3：外部 API（框架微服務）

**框架不在核心內，而是作為獨立微服務：**

```python
# langchain_service.py — 獨立 FastAPI 服務
from fastapi import FastAPI

app = FastAPI()

@app.post("/agent")
def agent(task: dict):
    """
    框架作為外部微服務
    核心通過 HTTP API 調用
    """
    agent = create_agent()
    result = agent.invoke(task["tokens"].decode())
    return {"tokens": result.encode(), "metadata": task["metadata"]}
```

**配置：**
```python
{
    "id": "langchain_agent_v1",
    "type": "api",
    "endpoint": "http://localhost:8000/agent"
}
```

**執行方式：**
```python
task = {"tokens": tokens.decode(), "metadata": metadata}

response = requests.post(
    config["endpoint"],
    json=task,
    timeout=timeout
)

result = response.json()
return result["tokens"].encode(), result["metadata"]
```

### 類型 4：LiteLLM 調用（統一 LLM 接口）

**配置：**
```python
{
    "id": "gpt4_v1",
    "type": "llm",
    "model": "gpt-4",
    "params": {"temperature": 0.7, "max_tokens": 2048}
}
```

**核心執行：**
```python
def call_llm_function(func_config, tokens, metadata):
    result = self.llm_client.call(
        model=func_config["model"],
        messages=[{"role": "user", "content": tokens.decode()}],
        **func_config.get("params", {})
    )
    return (
        result.choices[0].message.content.encode(),
        {**metadata, "tool": "litellm", "model": func_config["model"]}
    )
```

---

## 通信方式 & OS 層

### 方式 1：Shell（進程 + JSON）

```python
# 核心 ──── (subprocess + JSON) ──> Shell 腳本

result = subprocess.run(
    ["bash", script_path, json.dumps(task)],
    capture_output=True,
    timeout=timeout
)

# Shell 腳本處理 JSON，返回 JSON
output = json.loads(result.stdout.decode())
```

**優點：** 語言無關，易於集成任何工具
**缺點：** 進程開銷，JSON 序列化開銷

### 方式 2：IPC（Pipe/Queue — 長期運行的工具）

```python
# 對於長期運行的工具（如 Aider）
parent_conn, child_conn = multiprocessing.Pipe()

def tool_worker(conn):
    while True:
        task = conn.recv()
        if task is None:
            break
        result = process(task)
        conn.send(result)

# 核心 ──── (Pipe) ──> Tool Worker
#     <──── (Pipe) ──
```

**優點：** 低開銷，支持流式通信
**缺點：** 需要維護工具進程的生命週期

### 方式 3：HTTP API（微服務）

```python
# 對於獨立的框架服務

response = requests.post(
    "http://service:8000/endpoint",
    json=task,
    timeout=timeout
)

result = response.json()
```

**優點：** 完全解耦，易於部署和擴展
**缺點：** 網絡延遲，序列化開銷

### 方式 4：文件系統（簡單狀態共享）

```python
# 核心寫入任務
with open("task.json", "w") as f:
    json.dump(task, f)

# 工具執行
result = tool.process("task.json")

# 工具寫入結果
with open("result.json", "w") as f:
    json.dump(result, f)

# 核心讀取
with open("result.json") as f:
    result = json.load(f)
```

**優點：** 最簡單，無需額外依賴
**缺點：** 效率低，不支持流式通信

---

## 多客戶端架構

### 核心概念

核心系統**完全解耦於具體的輸入/輸出實現**。不同的客戶端（CLI、Desktop、Web、Android 等）獨立處理各自的輸入/輸出，通過統一的 **Token + Metadata** 接口與核心通信。

```
┌──────────────────────────────────────────────────────┐
│              Client Layer (Platform-Specific)         │
└──────────────────────────────────────────────────────┘

CLI Client        Desktop App        Website          Android App
(stdin/stdout)    (GUI + File)       (HTTP +          (Touch + Voice +
(+ file I/O)      (+ clipboard)      WebSocket)       Notification)
     │                 │                 │                 │
     └─────────────────┴─────────────────┴─────────────────┘
                       │
        ┌──────────────▼──────────────┐
        │ Token Stream with Metadata  │
        │ {tokens, metadata {...}}    │
        └──────────────┬──────────────┘
                       │
        ┌──────────────▼──────────────┐
        │  ai_core System             │
        │  make_model → bind → take   │
        │  + UX functions             │
        └──────────────┬──────────────┘
                       │
        ┌──────────────▼──────────────┐
        │ Output Token with Metadata  │
        │ {tokens, metadata {...}}    │
        └──────────────┬──────────────┘
                       │
     ┌────────┬────────┼────────┬────────┐
     ▼        ▼        ▼        ▼        ▼
    CLI    Desktop    Web    Android   Other
```

### Input Metadata 設計

```python
{
    "source": "voice" | "text" | "file" | "api",
    "client_id": "cli-1" | "desktop-1" | "web-1" | "android-1",
    "session_id": "primary" | "secondary_1" | ...,
    "user_signal": "important" | "normal" | "background",
    "timestamp": 1234567890,
    "context_objects": [...],          # 相關上下文
    "encoding": "utf-8" | "base64",
    "source_characteristics": {
        "has_noise": true,              # for voice
        "language": "zh-TW",
        "confidence": 0.95              # for voice-to-text
    }
}
```

### Output Metadata 設計

```python
{
    "format": "text" | "voice" | "binary" | "structured",
    "priority": "high" | "normal" | "low",
    "urgency": "immediate" | "batch" | "deferred",
    "target_session": "primary" | "secondary_1",
    "recommended_clients": ["desktop", "web", "android"],
    "voice_length": 5000,               # milliseconds
    "importance_score": 0.95,           # from is_important()
    "can_batch": true,
    "ttl": 300,                         # time-to-live
    "requires_user_attention": true
}
```

### 多會話並行示例

**用戶同時打開三個會話：**

```
Desktop App (session_id: "primary")
├─ Web Browser (session_id: "secondary_1")
└─ CLI Terminal (session_id: "secondary_2")
```

**輸入流：**

```python
# Desktop → Core
{
    "tokens": "讓我查詢今天的日程",
    "metadata": {
        "session_id": "primary",
        "client_id": "desktop-1",
        "user_signal": "important"
    }
}

# Web → Core
{
    "tokens": "翻譯這段文字",
    "metadata": {
        "session_id": "secondary_1",
        "client_id": "web-1",
        "user_signal": "normal"
    }
}
```

**輸出流：**

```python
# Core → Desktop (high priority)
{
    "tokens": "今天有三個會議：9點、2點、5點",
    "metadata": {
        "priority": "high",
        "target_session": "primary",
        "format": "text",
        "voice_length": 3000,
        "importance_score": 0.95
    }
}
→ Desktop: 立即播放語音 + 顯示文本

# Core → Web (normal priority)
{
    "tokens": "[翻譯結果]",
    "metadata": {
        "priority": "normal",
        "target_session": "secondary_1",
        "format": "text"
    }
}
→ Web: 顯示文本
```

---

## Layer 1 數據結構

### 通信格式（統一的 Token + Metadata）

```python
# 客戶端請求 → 核心
Request = {
    "tokens": bytes,              # 原始輸入（更接近原始文本）
    "metadata": dict              # 上下文信息和元數據
}

# 核心回應 → 客戶端
Response = {
    "tokens": bytes,              # 處理結果
    "metadata": dict              # 輸出元數據
}
```

### Token 表示：bytes

```python
# tokens: bytes
#
# 優點：
# - 符合「更接近原始文本」的設計理念
# - 支持多種內容：文本、二進制、語音等
# - 分詞責任完全交給 LLM（不是核心的責任）
# - 客戶端可自由選擇粒度（字符、詞、句子）
```

### Metadata 表示：dict（動態擴展）

```python
# metadata: dict（不用 dataclass）
#
# 為什麼不用 dataclass：
# - dict 足夠靈活，函數可動態添加/修改 metadata 欄位
# - dataclass 強制結構，不利於系統演進
# - 符合 LISP 精神：數據和代碼對稱
#
# 示例：
metadata = {
    "source": "voice",
    "session_id": "primary",
    "user_signal": "important",
    "denoised": True,           # 由函數動態添加
    "confidence": 0.95,          # 由函數動態添加
    ...                          # 可無限擴展
}
```

---

## Layer 1 函數定義

### BaseFunction 基類

```python
from abc import ABC, abstractmethod
from typing import Tuple, Dict, Any

class BaseFunction(ABC):
    """所有函數的基礎類和接口"""
    
    def __init__(self):
        # 函數的元數據
        self.id: str = "base_function"
        self.type: str = "processing"
        self.version: str = "1.0"
        
        # 資源特徵
        self.resource_profile = {
            "memory": {
                "self": 0,                  # 自身占用（bytes）
                "running": 0,               # 運行時占用
                "peak": 0                   # 峰值占用
            },
            "time": {
                "startup": 0,               # 啟動時間（ms）
                "actual_work": 0,           # 實際工作時間（ms）
                "teardown": 0               # 清理時間（ms）
            }
        }
        
        # 可發現性
        self.metadata = {
            "grouping": "general",
            "tags": [],
            "expanded_name": "base.function"
        }
        
        # 執行要求
        self.input_requirements = {
            "source_type": None,           # 只對特定來源有效
            "required_fields": [],         # 必須有的 metadata 欄位
            "preconditions": []            # 前置條件
        }
        
        # 產出內容
        self.output_produces = {
            # 產生的 metadata 欄位及其含義
        }
    
    @abstractmethod
    def __call__(self, tokens: bytes, metadata: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
        """
        函數簽名：(tokens, metadata) → (tokens, metadata)
        
        Args:
            tokens: 輸入的 bytes 數據
            metadata: 輸入的元數據字典
        
        Returns:
            Tuple[bytes, Dict]: (輸出 tokens，輸出 metadata)
        """
        pass
    
    def should_execute(self, metadata: Dict[str, Any]) -> bool:
        """判斷這個函數是否應該執行"""
        
        # 檢查 source_type
        if self.input_requirements.get("source_type"):
            if metadata.get("source") != self.input_requirements["source_type"]:
                return False
        
        # 檢查必需欄位
        for field in self.input_requirements.get("required_fields", []):
            if field not in metadata:
                return False
        
        return True
    
    def get_signature(self) -> Dict[str, Any]:
        """返回函數的簽名信息"""
        return {
            "id": self.id,
            "type": self.type,
            "input_type": "tokens + metadata",
            "output_type": "tokens + metadata",
            "resource_profile": self.resource_profile,
            "metadata": self.metadata
        }
```

### 具體函數實現示例

```python
class DenoiseVoiceFunction(BaseFunction):
    """清理語音噪音的函數"""
    
    def __init__(self):
        super().__init__()
        self.id = "denoise_voice_v1"
        self.type = "preprocessing"
        self.resource_profile["memory"]["self"] = 5_000_000    # 5MB
        self.resource_profile["time"]["actual_work"] = 100      # 100ms avg
        self.metadata["tags"] = ["voice", "preprocessing", "noise"]
        self.metadata["grouping"] = "audio_processing"
        
        # 執行要求
        self.input_requirements["source_type"] = "voice"
        self.output_produces["denoised"] = True
    
    def __call__(self, tokens: bytes, metadata: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
        # 檢查是否應該執行
        if not self.should_execute(metadata):
            return tokens, metadata
        
        # 實現去噪邏輯
        clean_tokens = self.denoise_impl(tokens)
        
        # 更新 metadata
        output_metadata = {
            **metadata,
            "denoised": True,
            "noise_level_before": metadata.get("noise_level", "unknown"),
            "noise_level_after": self.estimate_noise(clean_tokens)
        }
        
        return clean_tokens, output_metadata
    
    def denoise_impl(self, tokens: bytes) -> bytes:
        # 實現去噪
        pass
    
    def estimate_noise(self, tokens: bytes) -> float:
        # 估計噪音級別
        pass
```

---

## 函數元數據與自動組合

### 元數據的多維度

#### 資源消耗（Resource Characteristics）

```python
resource_profile = {
    "memory": {
        "self": 0,           # 函數自身占用
        "running": 0,        # 執行時的占用
        "peak": 0            # 峰值占用
    },
    "time": {
        "startup": 0,        # 啟動時間
        "actual_work": 0,    # 真正工作的時間
        "teardown": 0        # 清理時間
    }
}
```

#### 可發現性（Discoverability）

```python
metadata = {
    "id": "denoise_voice_v1",
    "type": "preprocessing",
    "expanded_name": "audio.denoise_voice",
    "tags": ["voice", "preprocessing", "noise"],
    "grouping": "audio_processing"
}
```

#### 語義分類（Semantic Classification）

```python
metadata = {
    "conceptual_purpose": "清理語音輸入中的背景噪音",
    "grouping": "audio_processing",
    "domain": "audio"
}
```

### 自動函數組合系統

```
Single Root Input (tokens + metadata)
    ↓
Function Registry (all available functions with metadata)
    ↓
Automatic Composition Engine
    ↓
Optimized Function Chain (auto-linked by output→input matching)
    ↓
Execution Pipeline (sync + chained)
```

### 組合核心概念

- **投資** — 建造很多小函數，每個都有清晰的簽名和完整元數據
- **自動鏈接** — 系統根據輸出類型匹配下一個函數的輸入類型
- **智能選擇** — 當多個函數符合類型時，根據資源特徵和語義優先級選擇

### 待釐清的問題

1. **多選擇時的決策邏輯** — 當多個函數都符合 output→input 類型時，如何選擇？
2. **鏈的終止條件** — 何時停止自動鏈接？
3. **環形依賴處理** — 如果 func_A 需要 func_B 的輸出，但 func_B 也需要 func_A 的輸出？

---

## 執行模型 & 調度

### V1（當前）：同步 + 串聯

```python
def process(self, request: Dict[str, Any]) -> Dict[str, Any]:
    """
    處理一個請求
    
    流程：
    1. 提取 tokens 和 metadata
    2. 根據 metadata 自動選擇函數鏈
    3. 順序執行（同步）
    4. 返回結果
    """
    tokens = request["tokens"]
    metadata = request["metadata"]
    
    # Step 1: 選擇函數鏈
    func_chain = self.select_function_chain(metadata)
    
    # Step 2: 順序執行（同步）
    for func in func_chain:
        tokens, metadata = func(tokens, metadata)
    
    # Step 3: 返回結果
    return {"tokens": tokens, "metadata": metadata}
```

### 函數鏈選擇

```python
def select_function_chain(self, metadata: Dict[str, Any]) -> List[BaseFunction]:
    """
    根據 metadata 自動選擇合適的函數鏈
    
    策略：
    - 根據 source 類型（voice/text/file）選擇預定義的鏈
    - 基於 metadata 中的特定欄位動態調整
    """
    
    # 預定義的函數鏈
    chains = {
        "voice": [
            "denoise_voice_v1",
            "contextualize_v1",
            "call_llm_v1",
            "format_output_voice_v1"
        ],
        "text": [
            "contextualize_v1",
            "call_llm_v1",
            "format_output_text_v1"
        ],
        "file": [
            "read_file_v1",
            "contextualize_v1",
            "call_llm_v1",
            "format_output_text_v1"
        ]
    }
    
    # 根據 source 獲取鏈
    source = metadata.get("source", "text")
    chain_ids = chains.get(source, chains["text"])
    
    # 構建函數對象列表，根據 should_execute 過濾
    result = []
    for func_id in chain_ids:
        if func_id in self.functions:
            func = self.functions[func_id]
            if func.should_execute(metadata):
                result.append(func)
    
    return result
```

### V1 執行特性

```python
# 特性 1：同步執行
# - 簡單、可預測
# - 適合單一會話
# - 響應時間可控

# 特性 2：串聯執行
# - 一個函數的輸出 = 下一個函數的輸入
# - 符合 UNIX pipe 的哲學
# - 易於調試和測試

# 特性 3：自動選擇
# - 基於 metadata 的 source 字段
# - 用戶無需指定函數鏈
# - 系統可根據上下文智能調整
```

### 後期優化路線圖

```
V1（現在）：同步 + 串聯
├─ 簡單、可預測
├─ 單會話支持
└─ 易於調試

V2（第二階段）：並行執行
├─ 支持 DAG（有向無環圖）
├─ 多個獨立函數鏈可並行
└─ 多會話支持

V3（第三階段）：非同步執行
├─ async/await 支持
├─ 高吞吐量
└─ 支持長流程（如網絡調用）

V4（後期）：動態調度
├─ 根據系統負載選擇執行策略
├─ 自適應優化
└─ 分佈式執行支持
```

### 任務狀態機

```
pending ──→ running ──→ waiting ──→ completed
            │                        ↑
            └────→ suspended ────────┘

- pending: 等待調度執行
- running: 正在 CPU 上執行
- waiting: 等待某個資源（如 LLM API 響應）
- suspended: 被調度器暫停
- completed: 執行完成
```

---

## 會話管理

### CoreEnvironment（LISP 風格的執行環境）

```python
from abc import ABC, abstractmethod
from typing import Dict, List, Any
import time

class CoreEnvironment:
    """
    LISP 風格的執行環境
    
    類似 LISP REPL，維護：
    - 函數的全局註冊表
    - 會話的本地上下文
    - 執行狀態
    """
    
    def __init__(self):
        # 全局函數註冊表
        self.functions: Dict[str, BaseFunction] = {}
        
        # 多會話管理
        self.session_contexts: Dict[str, Dict[str, Any]] = {}
        
        # 函數組合的索引（用於快速查找）
        self.type_index: Dict[str, List[str]] = {}    # type → [function_ids]
        self.tag_index: Dict[str, List[str]] = {}     # tag → [function_ids]
        
        # LiteLLM 客戶端
        self.llm_client = LiteLLMClient()
    
    def register_function(self, func: BaseFunction) -> None:
        """註冊一個函數"""
        self.functions[func.id] = func
        
        # 建立 type 索引
        func_type = func.type
        if func_type not in self.type_index:
            self.type_index[func_type] = []
        self.type_index[func_type].append(func.id)
        
        # 建立 tag 索引
        for tag in func.metadata.get("tags", []):
            if tag not in self.tag_index:
                self.tag_index[tag] = []
            self.tag_index[tag].append(func.id)
    
    def find_by_type(self, func_type: str) -> List[BaseFunction]:
        """O(1) 按類型查找函數"""
        func_ids = self.type_index.get(func_type, [])
        return [self.functions[fid] for fid in func_ids]
    
    def find_by_tag(self, tag: str) -> List[BaseFunction]:
        """O(1) 按標籤查找函數"""
        func_ids = self.tag_index.get(tag, [])
        return [self.functions[fid] for fid in func_ids]
    
    def get_or_create_session(self, session_id: str) -> Dict[str, Any]:
        """獲取或創建會話上下文"""
        if session_id not in self.session_contexts:
            self.session_contexts[session_id] = {
                "created_at": time.time(),
                "history": [],
                "variables": {},
                "metadata": {}
            }
        return self.session_contexts[session_id]
    
    def cleanup_session(self, session_id: str) -> None:
        """清理會話"""
        if session_id in self.session_contexts:
            del self.session_contexts[session_id]
    
    def process(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """主請求處理入口"""
        tokens = request["tokens"]
        metadata = request["metadata"]
        session_id = metadata.get("session_id", "default")
        
        # 1. 會話管理
        self.get_or_create_session(session_id)
        
        # 2. 決定調用哪個函數
        func_chain = self.select_function_chain(metadata)
        
        # 3. 執行函數
        for func in func_chain:
            tokens, metadata = func(tokens, metadata)
        
        # 4. 清理會話（如果需要）
        if metadata.get("session_end"):
            self.cleanup_session(session_id)
        
        return {
            "tokens": tokens,
            "metadata": metadata
        }
    
    def select_function_chain(self, metadata: Dict[str, Any]) -> List[BaseFunction]:
        """根據 metadata 選擇函數鏈"""
        
        chains = {
            "voice": [
                "denoise_voice_v1",
                "contextualize_v1",
                "call_llm_v1",
                "format_output_voice_v1"
            ],
            "text": [
                "contextualize_v1",
                "call_llm_v1",
                "format_output_text_v1"
            ]
        }
        
        source = metadata.get("source", "text")
        chain_ids = chains.get(source, chains["text"])
        
        result = []
        for func_id in chain_ids:
            if func_id in self.functions:
                func = self.functions[func_id]
                if func.should_execute(metadata):
                    result.append(func)
        
        return result
```

---

## 實現優先級

### Priority 1（必做，第一週）

基礎框架和最小可運行系統：

```python
# 1. BaseFunction 基類
class BaseFunction(ABC):
    @abstractmethod
    def __call__(self, tokens: bytes, metadata: dict) -> tuple[bytes, dict]:
        pass

# 2. CoreEnvironment 環境
class CoreEnvironment:
    def register(self, func: BaseFunction): ...
    def find_by_type(self, func_type: str): ...
    def process(self, request: dict) -> dict: ...

# 3. 簡單的函數鏈選擇邏輯
def select_function_chain(self, metadata: dict) -> List[BaseFunction]:
    pass

# 4. 同步串聯執行
def process(self, request: dict) -> dict:
    func_chain = self.select_function_chain(metadata)
    for func in func_chain:
        tokens, metadata = func(tokens, metadata)
    return {"tokens": tokens, "metadata": metadata}
```

### Priority 2（第二週）

具體的函數和測試：

```python
# 1. 具體函數實現
├─ DenoiseVoiceFunction
├─ ContextualizeFunction
├─ CallLLMFunction
└─ FormatOutputFunction

# 2. 單元測試
├─ 測試每個函數的邏輯
├─ 測試函數鏈的組合
└─ 測試 metadata 流動

# 3. Metadata 設計驗證
└─ 驗證 metadata 是否足夠表達業務邏輯
```

### Priority 3（第三週）

增強系統功能：

```python
# 1. 多會話管理
├─ session_contexts 的完善實現
├─ 會話隔離和狀態管理
└─ 並發會話的測試

# 2. 性能測試
├─ 吞吐量測試
├─ 延遲測試
└─ 記憶體使用測試

# 3. 決策：是否優化為非同步
└─ 根據性能測試結果決定
```

### Priority 4（後續）

高級功能：

```python
# 1. DAG 執行
├─ 支持多個獨立函數鏈並行
└─ 函數間的依賴管理

# 2. 動態函數選擇
├─ 基於性能特徵的選擇
├─ 基於用戶偏好的調整
└─ A/B 測試支持

# 3. 分佈式執行
├─ 函數可在不同機器執行
├─ 結果的聚合和合併
└─ 故障恢復
```

---

## 設計原則

### 1. LISP 風格

- 環境作為執行容器
- 函數作為一等公民
- 動態可擴展性

### 2. 元數據驅動

- 所有決策基於 metadata
- 不是硬編碼邏輯
- 易於演進和適應

### 3. 簡潔至上

- V1 優先簡單和可預測
- 後期根據需求優化
- 每個決策都是可逆的

### 4. Python 優先

- 快速迭代和驗證
- 後期遷移到 Rust（如需要）
- 動態特性便於探索

### 5. 測試驅動

- 每個函數都獨立可測
- 函數鏈的組合可測
- 集成測試驗證整體流程

### 6. 極簡核心

- 核心只做必要的事
- 每個組件只做一件事
- 標準通信方式
- 用戶控制決策
- 零框架綁定（除 LiteLLM）

---

## 成本管理

### 核心的責任（不包括）

❌ 不統計資源使用
❌ 不計算成本
❌ 不管理預算
❌ 不追蹤消費

### 用戶的責任

✅ 監控 LiteLLM 調用
✅ 監控外部 API 請求
✅ 監控 Shell 命令執行
✅ 設置告警和限制
✅ 計算成本

**用戶側的監控示例：**

```python
class UserCostManager:
    """用戶自己管理成本"""
    
    def __init__(self):
        self.litellm_calls = 0
        self.api_calls = {}
        self.shell_executions = 0
    
    def process_request(self, request):
        """包裝核心的調用"""
        
        # 調用核心
        result = core.process_request(request)
        
        # 記錄成本信息
        if result["metadata"]["tool"] == "litellm":
            self.litellm_calls += 1
        elif result["metadata"]["tool"] == "api":
            service = result["metadata"]["service"]
            self.api_calls[service] = self.api_calls.get(service, 0) + 1
        elif result["metadata"]["tool"] == "shell":
            self.shell_executions += 1
        
        return result
```

---

## 框架的位置（完全外部）

### LangChain

❌ 不在核心內
✅ 作為 HTTP Service 部署
✅ 核心通過 API 調用

### DSPy

❌ 不在核心內
✅ 作為 HTTP Service 部署
✅ 核心通過 API 調用

### CrewAI

❌ 不在核心內
✅ 作為 HTTP Service 部署
✅ 核心通過 API 調用

### Aider

❌ 不在核心內
✅ 通過 Shell 命令調用
✅ 核心通過 subprocess 執行

### LiteLLM

✅ **唯一在核心內的框架**
✅ 最小化依賴
✅ 統一 LLM 調用接口

---

## 完整核心代碼框架

```python
# core.py — ai_core 的完整實現（極簡版本）

import subprocess
import json
import requests
import litellm
from typing import Dict, Tuple, List, Any
from abc import ABC, abstractmethod
import time

class BaseFunction(ABC):
    """所有函數的基礎類"""
    
    def __init__(self):
        self.id: str = "base_function"
        self.type: str = "processing"
        self.resource_profile = {
            "memory": {"self": 0, "running": 0, "peak": 0},
            "time": {"startup": 0, "actual_work": 0, "teardown": 0}
        }
        self.metadata = {
            "tags": [],
            "grouping": "general"
        }
        self.input_requirements = {
            "source_type": None,
            "required_fields": [],
            "preconditions": []
        }
        self.output_produces = {}
    
    @abstractmethod
    def __call__(self, tokens: bytes, metadata: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
        pass
    
    def should_execute(self, metadata: Dict[str, Any]) -> bool:
        if self.input_requirements.get("source_type"):
            if metadata.get("source") != self.input_requirements["source_type"]:
                return False
        
        for field in self.input_requirements.get("required_fields", []):
            if field not in metadata:
                return False
        
        return True


class CoreEnvironment:
    """ai_core — 極簡核心實現"""
    
    def __init__(self):
        self.function_registry = {}
        self.session_manager = {}
        self.type_index = {}
        self.tag_index = {}
        self.llm = litellm
    
    def register_function(self, func_id: str, func_config: dict):
        """註冊函數"""
        self.function_registry[func_id] = func_config
    
    def register_function_object(self, func: BaseFunction):
        """註冊函數對象"""
        self.function_registry[func.id] = func
        
        # 建立索引
        func_type = func.type
        if func_type not in self.type_index:
            self.type_index[func_type] = []
        self.type_index[func_type].append(func.id)
        
        for tag in func.metadata.get("tags", []):
            if tag not in self.tag_index:
                self.tag_index[tag] = []
            self.tag_index[tag].append(func.id)
    
    def process_request(self, request: dict) -> dict:
        """主請求處理"""
        
        tokens = request["tokens"]
        metadata = request["metadata"]
        session_id = metadata.get("session_id", "default")
        
        # 1. 會話管理
        self._get_or_create_session(session_id)
        
        # 2. 決定調用哪個函數
        func_id = self._determine_function(tokens, metadata)
        
        # 3. 執行函數
        result_tokens, result_metadata = self._execute_function(
            func_id, tokens, metadata
        )
        
        # 4. 清理會話（如果需要）
        if metadata.get("session_end"):
            self._cleanup_session(session_id)
        
        return {
            "tokens": result_tokens,
            "metadata": result_metadata
        }
    
    def _execute_function(self, func_id: str, tokens: bytes, metadata: dict) -> Tuple[bytes, dict]:
        """執行函數（通過 OS 層）"""
        
        func_config = self.function_registry[func_id]
        func_type = func_config.get("type") if isinstance(func_config, dict) else func_config.type
        
        if func_type == "shell":
            return self._call_shell(func_config, tokens, metadata)
        elif func_type == "python":
            return self._call_python(func_config, tokens, metadata)
        elif func_type == "api":
            return self._call_api(func_config, tokens, metadata)
        elif func_type == "llm":
            return self._call_llm(func_config, tokens, metadata)
    
    def _call_shell(self, config: dict, tokens: bytes, metadata: dict) -> Tuple[bytes, dict]:
        """調用 Shell 命令"""
        
        task = {"tokens": tokens.decode(), "metadata": metadata}
        
        result = subprocess.run(
            ["bash", config["path"], json.dumps(task)],
            capture_output=True,
            timeout=metadata.get("constraints", {}).get("max_runtime_ms", 30000) / 1000
        )
        
        return result.stdout, {
            **metadata,
            "tool": "shell",
            "exit_code": result.returncode
        }
    
    def _call_python(self, config: dict, tokens: bytes, metadata: dict) -> Tuple[bytes, dict]:
        """調用本地 Python 函數"""
        
        func = config if isinstance(config, BaseFunction) else config.get("instance")
        return func(tokens, metadata)
    
    def _call_api(self, config: dict, tokens: bytes, metadata: dict) -> Tuple[bytes, dict]:
        """調用外部 API"""
        
        task = {"tokens": tokens.decode(), "metadata": metadata}
        
        response = requests.post(
            config["endpoint"],
            json=task,
            timeout=metadata.get("constraints", {}).get("max_runtime_ms", 30000) / 1000
        )
        
        result = response.json()
        return result["tokens"].encode(), result["metadata"]
    
    def _call_llm(self, config: dict, tokens: bytes, metadata: dict) -> Tuple[bytes, dict]:
        """調用 LiteLLM"""
        
        response = self.llm.completion(
            model=config["model"],
            messages=[{"role": "user", "content": tokens.decode()}],
            **config.get("params", {})
        )
        
        return (
            response.choices[0].message.content.encode(),
            {**metadata, "tool": "litellm", "model": config["model"]}
        )
    
    def _determine_function(self, tokens: bytes, metadata: dict) -> str:
        """智能決定調用哪個函數"""
        
        text = tokens.decode().lower()
        
        if "optimize" in text:
            return "dspy_optimize_v1"
        elif "agent" in text:
            return "langchain_agent_v1"
        else:
            return "gpt4_default_v1"
    
    def _get_or_create_session(self, session_id: str):
        """獲取或創建會話"""
        if session_id not in self.session_manager:
            self.session_manager[session_id] = {
                "created_at": time.time(),
                "context": []
            }
    
    def _cleanup_session(self, session_id: str):
        """清理會話"""
        if session_id in self.session_manager:
            del self.session_manager[session_id]
```

---

## 結語

**ai_core 的核心承諾：**

> **把 LLM 視為一個純函數，通過元數據驅動的自動組合，在多客戶端架構下，以簡潔優雅的方式架構 AI 應用。**

**核心特性：**

✅ 極簡（~200 行）
✅ 單一依賴（LiteLLM）
✅ 松耦合（OS 層通信）
✅ 易擴展（配置驅動）
✅ 成本透明（用戶管理）
✅ 語言無關（任何語言的工具）
✅ 開放生態（完全可組合）

每一層設計都是為了支持這個核心理念，每一個決策都有明確的理由，每一個待決策的問題都可以根據實際情況靈活應對。

**讓我們開始構建這個系統吧！**
