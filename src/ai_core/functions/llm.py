"""LLMFunction — wraps a single LLM call.

Closure (fixed at construction):
    - model: e.g. "claude-sonnet-4-6"
    - system_prompt: prepended to messages (skipped if empty)
    - temperature
    - max_tokens

Context interaction:
    - reads `context["llm_history"]` (defaults to [])
    - appends the new turn (user + assistant) and writes back

The actual LLM call goes through the global `llm_client` singleton.
"""

from typing import Any

from ai_core.functions.base import BaseFunction
from ai_core.types import Context, FunctionResult, Tokens


class LLMFunction(BaseFunction):
    """LLM-calling function."""

    def __init__(
        self,
        func_id: str,
        model: str,
        system_prompt: str = "",
        temperature: float = 0.7,
        max_tokens: int = 2048,
    ) -> None:
        """Construct an LLM function.

        Args:
            func_id: unique identifier
            model: LiteLLM-format model name
            system_prompt: optional system message prepended to chats
            temperature: sampling temperature
            max_tokens: max tokens to generate
        """
        super().__init__(func_id)
        self.closure = {
            "model": model,
            "system_prompt": system_prompt,
            "temperature": temperature,
            "max_tokens": max_tokens,
        }
        self.metadata["type"] = "llm"

    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult:
        """Send the input as a user message and return the assistant's reply.

        Steps:
            1. Read history from context (default []).
            2. If system_prompt is set and history is empty, prepend system message.
            3. Append {"role": "user", "content": tokens.decode()}.
            4. Call llm_client.call(model=..., messages=...).
            5. Append assistant response to history.
            6. Write history back to context["llm_history"].
            7. Return (response.encode(), context).
        """
        history = list(context.get("llm_history", []))

        if self.closure["system_prompt"] and not history:
            history.append({"role": "system", "content": self.closure["system_prompt"]})

        history.append({"role": "user", "content": tokens.decode()})

        # Import inside __call__ to ensure the mock from conftest.py takes effect
        from ai_core.llm_client import llm_client

        reply = llm_client.call(
            model=self.closure["model"],
            messages=list(history),
            temperature=self.closure["temperature"],
            max_tokens=self.closure["max_tokens"],
        )

        history.append({"role": "assistant", "content": reply})
        context["llm_history"] = history

        return reply.encode(), context

    def to_dict(self) -> dict[str, Any]:
        """Serialize. Closure is JSON-safe."""
        return super().to_dict()

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "LLMFunction":
        """Reconstruct from a dict produced by to_dict()."""
        closure = data["closure"]
        func = cls(
            func_id=data["id"],
            model=closure["model"],
            system_prompt=closure.get("system_prompt", ""),
            temperature=closure.get("temperature", 0.7),
            max_tokens=closure.get("max_tokens", 2048),
        )
        func.metadata = data["metadata"]
        return func
