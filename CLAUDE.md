# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 專案願景

用 LISP 式的函數式思維來架構 LLM 的呼叫與組合。把 LLM 視為一個純函數：

```
llm : tokens → tokens
```

所有複雜行為都是這個基本函數的組合與包裝，沒有魔法，只有 closure 與 transform。

## 核心抽象層

**Layer 1 — `make-model`：配置模型**

```python
def make_model(model, temperature, top_k, ...):
    def call(tokens):
        return call_llm(model, temperature, top_k, tokens)
    return call
```

把模型名稱與參數封裝進 closure，回傳一個純粹的 `tokens → tokens` 函數。

**Layer 2 — `bind`：綁定 context（偏函數應用）**

```python
def bind(context, model_fn):
    def call(input_tokens):
        return model_fn(context + input_tokens)
    return call
```

把 system prompt / few-shot examples 等 context 預先綁入函數，產生一個新的、更專化的函數。等同於 LISP 的 closure 或 partial application。

**Layer 3 — `take`：解析輸出**

```python
def take(schema, fn):
    def call(input_tokens):
        raw = fn(input_tokens)
        return parse(schema, raw)
    return call
```

把 LLM 的原始 token 輸出轉換為結構化資料（JSON → dataclass 等）。

**組合範例：**

```python
agent = take(json_schema,
         bind(system_prompt,
         make_model("claude-sonnet-4-6", temperature=0.7, top_k=40)))

result = agent("使用者的輸入")
```

每一層都是對前一層的包裝，整體就是函數組合。

## 系統架構（三層模型）

### Layer 1 — 函數定義

每個函數由三部分組成：
- **Closure** — 函數自身的數據（固定綁定）
- **Context** — 會話級的全局數據（動態變化，不屬於函數）
- **Metadata** — 函數的描述信息（資源特性、標籤、語義等）

函數類型：LLM、Shell、Calculate、Composite（S-expression 組合）

### Layer 2 — 函數管理

**FunctionRegistry**：管理已定義的函數
- 註冊、查找、註銷函數
- 追蹤依賴關係（垃圾回收風格）
- 建立索引支持快速查找
- 支持版本管理和命名空間

### Layer 3 — 客户端-服務端接口

**Core 是無狀態的函數執行服務**：
- 不管理會話（由客户端管理）
- 只管理函數和執行
- 支持多種通信方式（REST API、Python Module、System Pipe 等）
- **Core 應該能被序列化保存和便携使用**

## 參考文檔

- **SYSTEM_DESIGN.md** — 完整的三層架構設計（主要文檔）
- **SYSTEM_DESIGN_EXTRA.md** — 設計總結和待決策問題
- **old/** — 之前的設計迭代版本

## 設計原則

✅ LISP 風格 — 函數是一等公民，可組合和引用  
✅ 極簡核心 — Core 無狀態，只做函數管理和執行  
✅ 元數據驅動 — 所有決策基於 Metadata  
✅ 多方式通信 — 不依賴特定的傳輸協議  
✅ 客户端自主 — 每個會話獨立管理自己的上下文  
✅ 函數獨立 — 函數是黑盒，易於版本管理和替換

## 使用者資訊

- 繁體中文母語者，習慣用語音輸入（只支援英文），但**請一律以繁體中文回覆**。
