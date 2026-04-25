---
name: System Design 3 - Context Management Core
description: ai_core 的核心設計，從會話上下文管理的角度出發
type: design
updated: 2026-04-25
---

# SYSTEM_DESIGN_3 — Context Management Core

## 設計出發點

不從架構圖出發，而從**實際使用場景**出發：

- 用戶在終端輸入文字
- 同時進行多個會話（用 `/session XXX` 切換）
- 每個會話有獨立的對話歷史、臨時數據、本地函數定義
- 所有會話共享 LLM client、全局函數、系統資源

**核心問題：多個會話如何管理各自的上下文，同時不互相干擾，並共享單例資源（尤其是 LLM）？**

**設計理念：就像 OS 管理多個進程一樣**
- 全局函數 = 系統庫（所有進程都能調用）
- 會話 = 進程（各自有獨立的上下文空間）
- LLM client = CPU（共享資源，但先不做複雜調度）

---

## Session：會話的三層上下文

```python
class Session:
    """會話，包含獨立的上下文空間"""
    
    def __init__(self, session_id: str, persistent: bool = False):
        self.id = session_id
        self.persistent = persistent              # 臨時 or 持久到磁盤
        
        # Layer 1: LLM 對話歷史
        self.history: list[Message] = []          # [{"role": "user/assistant", "content": "..."}]
        
        # Layer 2: 本地函數環境（可覆蓋全局函數）
        self.local_functions: dict[str, Callable] = {}
        
        # Layer 3: 臨時變量/狀態（會話專用）
        self.local_vars: dict[str, Any] = {}
        
        # 元數據
        self.metadata = {
            "created_at": time.time(),
            "last_accessed": time.time(),
            "state": "active"  # active, suspended, destroyed, persisted
        }
```

**三層含義**：
1. **history** — LLM 對話記錄，長度會影響 context 成本
2. **local_functions** — 用戶在這個會話定義的函數，優先於全局函數
3. **local_vars** — 臨時狀態，如 `/set var_name value` 保存的變數

---

## 函數查找層級：Lexical Scope

當執行一個函數時，核心按以下順序查找：

```python
def resolve_function(self, func_name: str, session: Session) -> Callable:
    # 1. 先查會話的本地函數（優先級最高）
    if func_name in session.local_functions:
        return session.local_functions[func_name]
    
    # 2. 再查全局函數（優先級次之）
    if func_name in self.global_functions:
        return self.global_functions[func_name]
    
    # 3. 找不到則報錯
    raise FunctionNotFoundError(f"Unknown function: {func_name}")
```

**設計特點**：
- 會話可以用 **shadow** 技巧覆蓋全局函數（local override）
- 類似 Python 的作用域查找：local scope → global scope → built-ins
- 不支援跨會話函數調用（會話間是隔離的）

---

## CoreEnvironment：執行環境

```python
class CoreEnvironment:
    """ai_core 的中樞，管理會話和全局資源"""
    
    def __init__(self):
        # 全局層
        self.global_functions: dict[str, Callable] = {}
        self.llm_client = LiteLLMClient()  # 單例
        
        # 會話層
        self.sessions: dict[str, Session] = {}
        self.active_session_id: str = None
    
    def create_session(self, session_id: str, persistent: bool = False) -> Session:
        """創建新會話"""
        session = Session(session_id, persistent=persistent)
        self.sessions[session_id] = session
        self.active_session_id = session_id
        return session
    
    def switch_session(self, session_id: str) -> Session:
        """切換到另一個會話（不銷毀當前會話）"""
        if session_id not in self.sessions:
            raise SessionNotFoundError(f"Session {session_id} not found")
        
        # 標記舊會話為 suspended（暫停）
        if self.active_session_id:
            old = self.sessions[self.active_session_id]
            old.metadata["state"] = "suspended"
            old.metadata["last_accessed"] = time.time()
        
        # 激活新會話
        self.active_session_id = session_id
        session = self.sessions[session_id]
        session.metadata["state"] = "active"
        session.metadata["last_accessed"] = time.time()
        return session
    
    def process(self, request: dict) -> dict:
        """
        主請求處理入口
        
        request = {
            "tokens": bytes,          # 用戶輸入
            "metadata": {...}
        }
        """
        session = self.sessions[self.active_session_id]
        tokens = request["tokens"]
        
        # 執行函數（使用該會話的上下文）
        func_name = self._determine_function(tokens, session)
        func = self.resolve_function(func_name, session)
        
        output_tokens, output_metadata = func(tokens, session.local_vars)
        
        return {"tokens": output_tokens, "metadata": output_metadata}
```

---

## LLM 作為共享單例

LLM client 是單例，但每個會話有自己的對話歷史：

```python
class CoreEnvironment:
    
    def call_llm(self, user_input: str, session: Session) -> str:
        """
        調用 LLM，但傳入該會話的 history
        
        重點：
        - llm_client 是全局單例（所有會話共享）
        - 但 session.history 是會話私有
        - 結果自動追加到 session.history
        """
        # 將用戶輸入添加到會話歷史
        session.history.append({"role": "user", "content": user_input})
        
        # 調用 LLM（傳入該會話的完整歷史）
        response = self.llm_client.call(
            messages=session.history,
            model="claude-sonnet-4-6"
        )
        
        assistant_msg = response.choices[0].message.content
        session.history.append({"role": "assistant", "content": assistant_msg})
        
        # 檢查 context 長度（關鍵：只警告，不自動截斷）
        token_count = self._estimate_tokens(session.history)
        if token_count > self.context_limit:
            return assistant_msg, {
                "context_warning": f"Context length: {token_count}/{self.context_limit}",
                "context_limit_exceeded": True
            }
        
        return assistant_msg, {}
```

---

## 會話的生命週期

```
create
  ↓
active ←→ suspended (切換到另一個會話)
  ↓
  ├─ destroyed (臨時會話，進程結束)
  └─ persisted (持久會話，寫到磁盤)
```

**臨時會話**：進程結束時消失
```python
def create_session(self, "chat_1", persistent=False)
# 進程結束 → 會話消失
```

**持久會話**：可保存到磁盤，下次恢復
```python
def create_session(self, "chat_1", persistent=True)
# 可用 save_session("chat_1") 寫到文件
# 下次用 load_session("chat_1") 恢復
```

---

## Context 長度警告機制

核心**只負責監控**，不自動截斷：

```python
def _estimate_tokens(self, history: list[dict]) -> int:
    """估計 token 數（簡單實現：字符數 / 4）"""
    total = sum(len(msg.get("content", "")) for msg in history)
    return int(total / 4)  # 粗略估計

def check_context_length(self, session: Session) -> dict:
    """返回警告信息"""
    token_count = self._estimate_tokens(session.history)
    
    if token_count > self.context_limit:
        return {
            "status": "warning",
            "current_tokens": token_count,
            "limit": self.context_limit,
            "exceeded_by": token_count - self.context_limit,
            "suggestions": [
                "/compress  # 用 LLM 摘要舊對話",
                "/clear     # 清空歷史",
                "/forget N  # 遺忘最舊的 N 條消息"
            ]
        }
    return {"status": "ok", "current_tokens": token_count}
```

**核心策略**：
- 計算 context 長度 → 超過閾值 → 在 output metadata 附上警告
- 不自動截斷或 summarize（讓用戶決定）
- 用戶可執行 `/compress`、`/clear` 等命令

---

## 待決策問題

1. **持久化的存儲格式**
   - JSON？SQLite？MessagePack？
   - 怎樣序列化函數對象？

2. **函數的定義語言**
   - 只支持 Python？還是也支持 Shell / 其他語言？
   - 怎樣註冊用戶自定義函數？

3. **會話間是否共享某些狀態**
   - 目前設計是完全隔離
   - 是否允許會話間傳遞數據？

4. **LLM 的並發調用**
   - 當多個會話同時調用 LLM 時，怎樣處理？
   - 排隊執行（簡單）還是併發（複雜）？

---

## 設計原則

✅ **簡潔** — Session + 函數查找 + LLM 共享，就是全部
✅ **隔離** — 每個會話有獨立的上下文空間
✅ **共享** — 全局函數和 LLM 是共享資源
✅ **可恢復** — 持久會話可保存到磁盤
✅ **用戶中心** — Context 警告由用戶決定如何處理

---

讓我們開始實現這個設計！
