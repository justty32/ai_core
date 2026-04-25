---
name: System Design 4 - Core Architecture
description: ai_core 的三層架構：函數定義、函數管理、客户端-服務端接口
type: design
updated: 2026-04-25
---

# SYSTEM_DESIGN_4 — Core Architecture

ai_core 的三層架構：

1. **Layer 1 — 函數定義** — 如何定義一個函數
2. **Layer 2 — 函數管理** — 如何管理已定義的函數
3. **Layer 3 — 客户端-服務端接口** — 如何與 Core 通信

---

## Layer 1：函數定義

### 核心概念

每個函數由三部分組成：

- **Closure** — 函數自身擁有的數據（固定綁定，運行時不變）
  - 例：LLM 模型名稱、system prompt、shell 腳本路徑

- **Context** — 會話級的全局數據（動態、屬於會話，不屬於函數）
  - 例：LLM 對話歷史、臨時變數、會話狀態

- **Metadata** — 函數的描述信息（幫助系統理解函數）
  - 資源特性、可發現性、語義分類、執行要求、產出內容

### Metadata 設計

```python
metadata = {
    # 基本識別
    "id": "denoise_voice_v1",
    "type": "preprocessing",
    "version": "1.0",
    
    # 可發現性
    "expanded_name": "audio.denoise_voice",
    "tags": ["voice", "preprocessing", "audio"],
    "grouping": "audio_processing",
    
    # 語義分類
    "conceptual_purpose": "清理語音中的背景噪音",
    "domain": "audio",
    
    # 資源特性
    "resource_profile": {
        "memory": {"self": 5_000_000, "running": 10_000_000, "peak": 15_000_000},
        "time": {"startup": 500, "actual_work": 100, "teardown": 50}
    },
    
    # 執行要求
    "input_requirements": {
        "source_type": "voice",
        "required_fields": ["session_id"],
        "preconditions": []
    },
    
    # 產出
    "output_produces": {
        "denoised": "boolean",
        "noise_level_after": "float"
    }
}
```

### BaseFunction 基類

```python
from abc import ABC, abstractmethod
from typing import Tuple, Dict, Any

class BaseFunction(ABC):
    """所有函數的基類"""
    
    def __init__(self, func_id: str):
        self.id = func_id
        self.closure: Dict[str, Any] = {}        # 函數自身的數據
        self.metadata: Dict[str, Any] = {...}    # 函數的描述信息
    
    @abstractmethod
    def __call__(self, tokens: bytes, context: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
        """
        函數簽名：(tokens, context) → (tokens, context)
        
        Args:
            tokens: 輸入數據
            context: 會話級的全局上下文（可讀寫）
        
        Returns:
            (output_tokens, updated_context)
        """
        pass
```

### 四種函數類型

#### 1. LLM Function

```python
class LLMFunction(BaseFunction):
    def __init__(self, func_id: str, model: str, system_prompt: str = ""):
        super().__init__(func_id)
        
        # Closure — LLM 的固定配置
        self.closure = {
            "model": model,
            "system_prompt": system_prompt,
            "temperature": 0.7,
            "max_tokens": 2048
        }
        
        self.metadata["type"] = "llm"
    
    def __call__(self, tokens: bytes, context: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
        # 從 context 讀取歷史，調用 LLM，寫回歷史
        history = context.get("llm_history", [])
        messages = history + [{"role": "user", "content": tokens.decode()}]
        
        response = llm_client.call(
            model=self.closure["model"],
            messages=messages,
            temperature=self.closure["temperature"]
        )
        
        result = response.choices[0].message.content
        context["llm_history"] = messages + [{"role": "assistant", "content": result}]
        
        return result.encode(), context
```

#### 2. Shell Function

```python
class ShellFunction(BaseFunction):
    def __init__(self, func_id: str, script_path: str):
        super().__init__(func_id)
        self.closure = {"script_path": script_path}
        self.metadata["type"] = "shell"
    
    def __call__(self, tokens: bytes, context: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
        result = subprocess.run(
            ["bash", self.closure["script_path"], tokens.decode()],
            capture_output=True,
            timeout=30
        )
        return result.stdout, context
```

#### 3. Calculate Function

```python
class CalculateFunction(BaseFunction):
    def __init__(self, func_id: str, python_func: Callable):
        super().__init__(func_id)
        self.closure = {"func": python_func}
        self.metadata["type"] = "calculate"
    
    def __call__(self, tokens: bytes, context: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
        result = self.closure["func"](tokens)
        return result if isinstance(result, bytes) else str(result).encode(), context
```

#### 4. Composite Function（S-expression）

```python
class CompositeFunction(BaseFunction):
    """由其他函數組合而成，展示 S-expression 的理念"""
    
    def __init__(self, func_id: str, func_chain: List[str]):
        super().__init__(func_id)
        self.closure = {"func_chain": func_chain}
        self.metadata["type"] = "composite"
    
    def __call__(self, tokens: bytes, context: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
        current = tokens
        for func_id in self.closure["func_chain"]:
            func = function_registry.get(func_id)
            current, context = func(current, context)
        return current, context
```

---

## Layer 2：函數管理

### FunctionRegistry — 函數註冊表

```python
class FunctionRegistry:
    """
    管理已定義的函數
    
    職責：
    - 註冊/註銷函數
    - 查找函數（按 ID、type、tag）
    - 追蹤依賴關係
    - 管理生命週期
    """
    
    def __init__(self):
        self.functions: Dict[str, BaseFunction] = {}
        self.references: Dict[str, Set[str]] = {}  # func_id → {引用它的 func_ids}
        self.by_type: Dict[str, List[str]] = {}
        self.by_tag: Dict[str, List[str]] = {}
    
    def register(self, func: BaseFunction) -> None:
        """註冊函數"""
        self.functions[func.id] = func
        
        # 建立索引
        func_type = func.metadata.get("type")
        if func_type:
            self.by_type.setdefault(func_type, []).append(func.id)
        
        for tag in func.metadata.get("tags", []):
            self.by_tag.setdefault(tag, []).append(func.id)
        
        # 追蹤依賴（Composite 函數）
        if func_type == "composite":
            for ref_func_id in func.closure.get("func_chain", []):
                self.references.setdefault(ref_func_id, set()).add(func.id)
    
    def unregister(self, func_id: str) -> None:
        """
        註銷函數
        
        只有當沒有其他函數引用它時，才能刪除
        （類似垃圾回收）
        """
        if self.references.get(func_id):
            raise FunctionInUseError(
                f"Cannot unregister {func_id}: still referenced by {self.references[func_id]}"
            )
        del self.functions[func_id]
    
    def get(self, func_id: str) -> BaseFunction:
        """按 ID 獲取函數"""
        return self.functions.get(func_id)
    
    def find_by_type(self, func_type: str) -> List[BaseFunction]:
        """按類型查找"""
        return [self.functions[fid] for fid in self.by_type.get(func_type, [])]
    
    def find_by_tag(self, tag: str) -> List[BaseFunction]:
        """按標籤查找"""
        return [self.functions[fid] for fid in self.by_tag.get(tag, [])]
```

---

## Layer 3：客户端-服務端接口

### 架構概念

**Core 是一個無狀態的函數執行服務**
- 不管理會話
- 只管理函數（FunctionRegistry + LLMClient）
- 接收請求，執行函數，返回結果

**會話由客户端管理**
- 可以是 CLI 進程、Desktop App、Web、Android 等
- 各自維護自己的 context、history、臨時數據
- 通過 API 調用 Core 的函數
- 完全獨立，Core 無需知道會話存在

```
┌────────────────────────────────────┐
│  Core Service                      │
├────────────────────────────────────┤
│ • FunctionRegistry                 │
│ • LLMClient（單例）                │
│ • execute(func_id, tokens, context)│
└────────────────────────────────────┘
         ↑        ↑        ↑
    (REST API)
         │        │        │
    ┌────┴─┐  ┌──┴───┐  ┌─┴────┐
   CLI   Desktop   Web    ...
 (process)(process)(JS)
    
    Each client session:
    ├─ context（本地維護）
    ├─ history（本地維護）
    └─ calls Core API
```

### 統一接口

```python
# Core 的執行接口（可通過不同方式訪問）
def execute(func_id: str, tokens: bytes, context: Dict[str, Any]) -> Tuple[bytes, Dict[str, Any]]:
    """
    執行一個函數
    
    Args:
        func_id: 函數 ID
        tokens: 輸入數據
        context: 會話的上下文（客户端維護）
    
    Returns:
        (output_tokens, updated_context)
    """
    func = function_registry.get(func_id)
    if not func:
        raise FunctionNotFoundError(f"Function {func_id} not found")
    
    return func(tokens, context)
```

### 通信方式（簡化示例）

#### REST API（主要方式）

```
POST /execute
Content-Type: application/json

{
    "func_id": "llm_v1",
    "tokens": "base64_encoded_bytes",
    "context": {
        "llm_history": [...],
        "temp_vars": {...}
    }
}

Response:
{
    "tokens": "base64_encoded_bytes",
    "context": {
        "llm_history": [...],
        "temp_vars": {...}
    }
}
```

#### Python Module（直接導入）

```python
from ai_core import core_instance

tokens, context = core_instance.execute(
    func_id="llm_v1",
    tokens=b"hello",
    context=session_context
)
```

#### System Pipe（subprocess）

```bash
# 客户端進程向 Core 發送請求
echo '{"func_id":"llm_v1","tokens":"...","context":{...}}' | core_service
# 讀取結果
```

### Core 的便携性

Core 應該能被完整地保存到文件並在任何環境中恢復：
- 函數註冊表的序列化
- 所有函數定義的保存
- Closure 數據的持久化
- 不依賴特定路徑或環境變量

（詳細實現細節在後續討論）

---

## 三層架構總結

| Layer | 核心 | 職責 |
|-------|------|------|
| **L1** | 函數定義 | BaseFunction、Closure、Metadata、四種類型、S-expression |
| **L2** | 函數管理 | FunctionRegistry、索引、依賴追蹤、生命週期 |
| **L3** | 客户端-服務端接口 | 統一執行接口、多種通信方式、Core 便携性 |

---

## 設計原則

✅ **極簡核心** — Core 無狀態，只做函數執行
✅ **客户端自主** — 會話由客户端管理，完全獨立
✅ **多方式通信** — REST、Python Module、System Pipe 等
✅ **天然分布式** — 客户端可在不同機器上
✅ **便携可復現** — Core 可完整保存和恢復

---

讓我們繼續改進和討論！
