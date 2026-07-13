#!/usr/bin/env python3
"""llm_call — 把 LLM 視為一個（隨機的）函式：``llm_call(str) -> str``。

對應 ai_core/CLAUDE.md 元件 2「LLM Calling Packing」與 thinking.md 的核心隱喻：
把 LLM 呼叫當成函式，再疊 context 與 post-processing 形成更具語意的新函式。

== 兩件事 ==
1. **基底**：``llm_call(prompt) -> text``。背後是可插拔 backend（本地/遠端/mock）。
2. **打包（packing）**：``bind(...)`` 把 system/prefix/suffix/postprocess 疊上去，
   產出一個新的 ``f(str)->str``。這正是 CLAUDE.md 的範例：

       coding_q = bind(system="you are a professor of coding, and...",
                       postprocess=lambda o: o + " -- at 20240505")
       coding_q("how to sort a list?")

== 與「馴化隨機性」的關係 ==
llm_call 是這套系統裡唯一的**非確定性函式**。馴化它的手段不寫在這裡，而是 lib/compose.py
的通用組合子（retry_until_valid 拒絕採樣、vote 自一致投票、memoize 強制確定性…）。
設計上刻意如此：**LLM 只是「一個比較吵的函式」，馴化它用的是組合任意函式的同一套組合子。**
詳見 docs/llm_taming_framework.md。

== Backend ==
本機沒有真 LLM，故預設 EchoBackend；ScriptedBackend / FnBackend 用來在測試裡模擬
「同一輸入、不同輸出」的隨機性，好讓 compose 的馴化組合子有東西可馴。

**真接 API**（2026-06-08）：`OpenAIBackend`（OpenAI 相容 `/chat/completions`，吃本地
ollama / llama.cpp / vLLM / OpenRouter）與 `AnthropicBackend`（`/v1/messages`）兩個真 backend
已實作，都走 `lib/call.Http`（純 urllib、零外部相依），上層 bind/compose 完全不變。
用 `backend_from_env()` 依環境變數挑 backend（未設定則回 EchoBackend，離線/測試友善）。
"""

from __future__ import annotations

import json
import os
from typing import Any, Callable, Mapping


class Backend:
    """LLM backend 介面。實作 complete(prompt, **opts) -> str 即可。"""

    def complete(self, prompt: str, **opts: Any) -> str:  # pragma: no cover - 抽象
        raise NotImplementedError


class EchoBackend(Backend):
    """把 prompt 回顯——預設 backend，無 LLM 時用。"""

    def complete(self, prompt: str, **opts: Any) -> str:
        return f"echo: {prompt}"


class ScriptedBackend(Backend):
    """依序吐出預設回應（循環）。用來在測試裡模擬非確定性。

    responses 元素可為字串，或 ``fn(prompt) -> str``。
    """

    def __init__(self, responses: list):
        self.responses = list(responses)
        self.i = 0

    def complete(self, prompt: str, **opts: Any) -> str:
        r = self.responses[self.i % len(self.responses)]
        self.i += 1
        return r(prompt) if callable(r) else r


class FnBackend(Backend):
    """用任意 ``fn(prompt, opts) -> str`` 當 backend（最自由）。"""

    def __init__(self, fn: Callable[[str, dict], str]):
        self.fn = fn

    def complete(self, prompt: str, **opts: Any) -> str:
        return self.fn(prompt, opts)


class OpenAIBackend(Backend):
    """OpenAI 相容 ``/chat/completions``。

    適配**本地小模型**（ollama / llama.cpp / vLLM）與相容代理（OpenRouter…）——這正是
    roadmap「假設只剩便宜本地小模型」的目標 backend。走 ``lib/call.Http``（urllib，零相依）。

    base_url 給 API 根（含相容前綴），例如：
        ollama   → http://localhost:11434/v1
        llama.cpp→ http://localhost:8080/v1
    本 backend 會接上 ``/chat/completions``。prompt 整段當成單一 user message
    （bind() 已把 system/prefix/suffix 疊進 prompt，這裡不再拆）。
    """

    def __init__(self, base_url: str, model: str, api_key: str | None = None,
                 timeout: float = 60.0, max_tokens: int = 1024):
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.api_key = api_key
        self.timeout = timeout
        self.max_tokens = max_tokens

    def complete(self, prompt: str, **opts: Any) -> str:
        from lib import call  # 延遲 import，避免只用 EchoBackend 時付出 trace 相依

        payload: dict[str, Any] = {
            "model": self.model,
            "messages": [{"role": "user", "content": prompt}],
            "max_tokens": opts.get("max_tokens", self.max_tokens),
        }
        for k in ("temperature", "top_p", "stop", "seed"):
            if k in opts:
                payload[k] = opts[k]
        headers = {"Content-Type": "application/json"}
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"
        resp = call.Http(self.base_url + "/chat/completions", headers=headers,
                         timeout=self.timeout).call(json.dumps(payload, ensure_ascii=False))
        data = json.loads(resp)
        return data["choices"][0]["message"]["content"]


class AnthropicBackend(Backend):
    """Anthropic Messages API ``/v1/messages``。走 ``lib/call.Http``（urllib，零相依）。

    base_url 給 API 根（如 ``https://api.anthropic.com``），會接上 ``/v1/messages``。
    回應的 content 是 block 陣列，這裡串接所有 text block。
    """

    def __init__(self, base_url: str, model: str, api_key: str | None = None,
                 timeout: float = 60.0, max_tokens: int = 1024,
                 version: str = "2023-06-01"):
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.api_key = api_key
        self.timeout = timeout
        self.max_tokens = max_tokens
        self.version = version

    def complete(self, prompt: str, **opts: Any) -> str:
        from lib import call  # 延遲 import（同 OpenAIBackend）

        payload: dict[str, Any] = {
            "model": self.model,
            "max_tokens": opts.get("max_tokens", self.max_tokens),
            "messages": [{"role": "user", "content": prompt}],
        }
        for k in ("temperature", "top_p", "stop_sequences"):
            if k in opts:
                payload[k] = opts[k]
        headers = {"Content-Type": "application/json",
                   "anthropic-version": self.version}
        if self.api_key:
            headers["x-api-key"] = self.api_key
        resp = call.Http(self.base_url + "/v1/messages", headers=headers,
                         timeout=self.timeout).call(json.dumps(payload, ensure_ascii=False))
        data = json.loads(resp)
        return "".join(b.get("text", "") for b in data.get("content", [])
                       if b.get("type") == "text")


def backend_from_env(env: Mapping[str, str] | None = None) -> Backend:
    """依環境變數挑 backend；未設定 / 未知 provider 則回 EchoBackend（離線/測試友善）。

    環境變數（共用）：
        AI_CORE_LLM_PROVIDER   openai | anthropic | echo（預設 echo）
        AI_CORE_LLM_BASE_URL   API 根 URL
        AI_CORE_LLM_MODEL      模型名
        AI_CORE_LLM_API_KEY    金鑰（本地模型常可省略）
        AI_CORE_LLM_MAX_TOKENS 生成上限（選填）

    這是 entry manager 與 idea 工具決定「打哪個真 LLM」的單一入口——把 provider 選擇
    集中在環境，工具與 bind() 程式碼完全不必改（呼應元件 1「統一 LLM 呼叫入口」）。
    """
    e = os.environ if env is None else env
    provider = (e.get("AI_CORE_LLM_PROVIDER") or "echo").lower()
    base_url = e.get("AI_CORE_LLM_BASE_URL")
    model = e.get("AI_CORE_LLM_MODEL")
    api_key = e.get("AI_CORE_LLM_API_KEY")
    max_tokens = int(e["AI_CORE_LLM_MAX_TOKENS"]) if e.get("AI_CORE_LLM_MAX_TOKENS") else 1024

    if provider == "openai":
        return OpenAIBackend(base_url or "http://localhost:11434/v1",
                             model or "llama3", api_key, max_tokens=max_tokens)
    if provider == "anthropic":
        return AnthropicBackend(base_url or "https://api.anthropic.com",
                                model or "claude-haiku-4-5-20251001", api_key,
                                max_tokens=max_tokens)
    return EchoBackend()


_default_backend: Backend = EchoBackend()


def set_default_backend(backend: Backend) -> None:
    global _default_backend
    _default_backend = backend


def llm_call(prompt: str, *, backend: Backend | None = None, **opts: Any) -> str:
    """基底：把 prompt 丟給 backend，回文字。

    opts（如 temperature / seed / max_tokens）原樣轉給 backend——是否支援由 backend 決定。
    """
    return (backend or _default_backend).complete(prompt, **opts)


def bind(
    *,
    system: str | None = None,
    prefix: str | None = None,
    suffix: str | None = None,
    postprocess: Callable[[str], str] | None = None,
    backend: Backend | None = None,
    **opts: Any,
) -> Callable[[str], str]:
    """把 context 與 post-processing 疊在 llm_call 之上，產出新的 ``f(str)->str``。

    這就是 CLAUDE.md「在基底上疊加 context 與 post-processing 形成新函式」的落地。
    產出的函式是無狀態 one-shot，可直接餵給 lib/compose 的組合子。
    """
    def f(text: str) -> str:
        parts: list[str] = []
        if system:
            parts.append(system)
        if prefix:
            parts.append(prefix)
        parts.append(text)
        if suffix:
            parts.append(suffix)
        prompt = "\n".join(parts)
        out = llm_call(prompt, backend=backend, **opts)
        if postprocess:
            out = postprocess(out)
        return out

    return f
