---
name: System Design Conclusion - Final Architecture
description: ai_core 系統設計的最終版本，整合所有決策
type: design
updated: 2026-04-25
---

# ai_core — System Design Conclusion

**最後更新**：2026-04-25  
**狀態**：Final Design（可開始詳細規劃和實現）

---

## 核心願景

用 **LISP 式的函數式思維** 架構 LLM 的調用與組合，把 LLM 視為一個純函數：

```
llm : tokens → tokens
```

所有複雜行為都是這個基本函數的組合與包裝，沒有魔法，只有 **closure 與 transform**。

### 三層核心抽象

```python
# Layer 0 — 基本構件
def make_model(model, temperature, top_k, ...):
    def call(tokens):
        return call_llm(model, temperature, top_k, tokens)
    return call

# Layer 1 — 綁定 context（偏函數應用）
def bind(context, model_fn):
    def call(input_tokens):
        return model_fn(context + input_tokens)
    return call

# Layer 2 — 解析輸出
def take(schema, fn):
    def call(input_tokens):
        raw = fn(input_tokens)
        return parse(schema, raw)
    return call

# 組合範例
agent = take(json_schema,
         bind(system_prompt,
         make_model("claude-sonnet-4-6", temperature=0.7)))

result = agent("使用者的輸入")
```

---

## 系統架構：三層模型

### Layer 1 — 函數定義

**定義一個函數 = 寫一個 Python class**

每個函數由三部分組成：

#### Closure（函數自身的數據，固定不變）
```python
# 例：LLM 函數的 closure
{
    "model": "claude-sonnet-4-6",
    "system_prompt": "You are an expert...",
    "temperature": 0.7,
    "max_tokens": 2048
}
```

#### Context（會話級的全局數據，動態變化）
```python
# 不屬於函數，屬於會話
{
    "llm_history": [{role, content}, ...],
    "temp_vars": {...},
    "session_id": "primary",
    "session_state": "active"
}
```

#### Metadata（函數的描述信息）
```python
{
    # 基本識別
    "id": "denoise_voice_v1",
    "type": "preprocessing",
    "version": "1.0",
    
    # 可發現性
    "expanded_name": "audio.denoise_voice",
    "tags": ["voice", "preprocessing"],
    
    # 語義分類
    "conceptual_purpose": "清理語音中的背景噪音",
    "domain": "audio",
    
    # 資源特性
    "resource_profile": {
        "memory": {"self": 5MB, "running": 10MB, "peak": 15MB},
        "time": {"startup": 500ms, "actual_work": 100ms, "teardown": 50ms}
    },
    
    # 執行要求
    "input_requirements": {
        "source_type": "voice",
        "required_fields": ["session_id"],
        "preconditions": []
    }
}
```

#### BaseFunction 基類

```python
class BaseFunction(ABC):
    def __init__(self, func_id: str):
        self.id = func_id
        self.closure: Dict[str, Any] = {}
        self.metadata: Dict[str, Any] = {...}
    
    @abstractmethod
    def __call__(self, tokens: bytes, context: Dict[str, Any]) 
        -> Tuple[bytes, Dict[str, Any]]:
        """(tokens, context) → (tokens, context)"""
        pass
```

#### 四種函數類型

1. **LLM Function** — 調用 LLM
   - Closure：模型名、system prompt、溫度等
   - 讀寫：context 中的 llm_history

2. **Shell Function** — 執行 Shell 命令
   - Closure：腳本路徑
   - 副作用：執行外部命令

3. **Calculate Function** — 包裝 Python 函數
   - Closure：Python 函數本身
   - 用途：純計算或有副作用的操作

4. **Composite Function** — 組合其他函數
   - Closure：函數 ID 列表
   - S-expression 思想：函數的優雅組合
   ```python
   (denoise (contextualize (call_llm tokens)))
   ```

---

### Layer 2 — 函數管理

**管理已定義的函數**

#### FunctionRegistry

```python
class FunctionRegistry:
    # 註冊/查找/註銷函數
    # 追蹤依賴關係（垃圾回收）
    # 建立索引（快速查找）
```

**職責**：
- 存儲所有已定義的函數
- 按 ID、type、tag 查找
- 追蹤 Composite 函數的依賴
- 防止刪除仍被引用的函數（類似垃圾回收）
- 支持版本管理和命名空間

**關鍵操作**：
```python
registry.register(func)           # 註冊函數
registry.get(func_id)              # 查找函數
registry.find_by_type("llm")       # 按類型查找
registry.find_by_tag("voice")      # 按標籤查找
registry.unregister(func_id)       # 註銷（只有沒被引用時才能刪除）
```

---

### Layer 3 — 客户端-服務端接口

**Core 是無狀態的函數執行服務**

#### 架構特點

```
Core Service (函數管理 + 執行)
  ├─ FunctionRegistry
  ├─ LLMClient（單例）
  └─ execute(func_id, tokens, context)

              ↕ API / Module / Pipe

多個獨立的客户端會話
  ├─ CLI 進程
  ├─ Desktop App 進程
  ├─ Web 頁面
  └─ Android App
  
每個會話：
  ├─ context（本地維護）
  ├─ history（本地維護）
  └─ calls Core API
```

#### Core 的責任

✅ **應該做**：
- 管理函數註冊表
- 執行函數
- 提供統一的執行接口

❌ **不應該做**：
- 管理會話
- 管理用戶輸入/輸出
- 知道什麼是 CLI、Web、Desktop

#### 客户端的責任

✅ **應該做**：
- 管理自己的 context
- 處理用戶輸入
- 調用 Core API
- 呈現輸出

❌ **不應該做**：
- 管理函數
- 決定執行邏輯

#### 通信方式

```
REST API (主要)
POST /execute
{
    "func_id": "llm_v1",
    "tokens": "base64_encoded",
    "context": {...}
}

Python Module (直接導入)
core.execute(func_id, tokens, context)

System Pipe (subprocess 通信)
echo '{"func_id": "llm_v1", ...}' | core_service
```

#### Core 的便携性

Core 應該能被完整地保存到文件（**待詳細設計**）：
- 函數註冊表的序列化
- 所有函數定義的保存
- Closure 數據的持久化
- 跨平台/跨環境使用

---

## 設計原則

### 1. LISP 風格
- 函數是一等公民
- 函數可以被組合和引用
- 環境作為執行容器

### 2. 極簡核心
- Core 只做必要的事
- 無狀態（或狀態少）
- 單一職責

### 3. 元數據驅動
- Metadata 描述函數特性
- 幫助系統進行智能決策
- 支持自動發現和組合

### 4. 多方式通信
- REST API、Python Module、System Pipe
- 不依賴特定的傳輸協議
- 靈活部署和集成

### 5. 客户端自主
- 每個會話獨立管理自己的上下文
- Core 無需知道會話存在
- 天然支持多進程/多機器

### 6. 函數獨立
- 函數是黑盒，可獨立測試
- 函數間通過標準接口通信
- 易於版本管理和替換

---

## 與之前設計的關係

### 來自 SYSTEM_DESIGN_1、2、3 的概念

**保留**：
- 三層核心抽象（make_model、bind、take）
- Metadata 的多維度設計
- 函數的四種類型分類
- 多客户端的分離思想
- 資源特性的定義

**調整**：
- 不再在 Core 中管理會話（改由客户端管理）
- 簡化了 Context Management（改為客户端責任）
- 澄清了 Closure vs Context 的區分

**棄用**：
- Layer 3 的虛擬會話管理
- 複雜的資源調度邏輯（暫時）
- 分佈式執行的設計（暫時）

---

## 待詳細設計的問題

### Layer 1 相關
1. 函數間的 S-expression 如何動態組合？
2. 是否支持條件分支和循環？
3. 如何驗證 metadata 的完整性？

### Layer 2 相關
1. 版本管理的具體策略
2. 函數的動態註冊機制
3. 依賴解析和衝突處理

### Layer 3 相關
1. Core 的序列化格式（JSON？Pickle？）
2. REST API 的完整 spec
3. Core 在不同環境中的部署方式

---

## 下一步

1. **詳細設計** — 對每一層進行詳細的技術設計
2. **實現規劃** — 確定實現的優先級和路線圖
3. **原型開發** — 實現最小可運行版本
4. **迭代改進** — 基於實踐反饋調整設計

---

## 設計文檔地圖

- **SYSTEM_DESIGN_4.md** — 詳細的三層架構實現
- **SYSTEM_DESIGN_CONCLUSION.md** — 本文件，最終總結
- （後續補充更多實現細節的文檔）

---

> **ai_core 的核心承諾：**  
> 把 LLM 視為一個純函數，通過函數組合和管理，  
> 在無狀態 Core + 獨立客户端的架構下，  
> 以簡潔優雅的方式架構 AI 應用。

讓我們開始實現吧！
