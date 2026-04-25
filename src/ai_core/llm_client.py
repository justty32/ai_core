"""Singleton wrapper around LiteLLM.

LiteLLM provides a unified `completion()` interface across all major LLM
providers (Anthropic, OpenAI, Google, etc.). We wrap it in a singleton so:

1. There's one place to configure provider credentials.
2. All function instances share the same client (cheap, but tidier).
3. Tests can monkeypatch the singleton instance for isolation.

Usage:
    from ai_core.llm_client import llm_client

    text = llm_client.call(
        model="claude-sonnet-4-6",
        messages=[{"role": "user", "content": "hello"}],
    )
"""

from typing import Any


class LLMClient:
    """Singleton LiteLLM wrapper.

    Construction returns the same instance every time (`__new__` enforces this).
    All methods delegate to `litellm.completion()`.
    """

    _instance: "LLMClient | None" = None

    def __new__(cls) -> "LLMClient":
        """Return the singleton instance, constructing on first call.

        Singleton boilerplate is implemented here so the module is
        importable before the rest of the class is filled in.
        """
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def call(
        self,
        model: str,
        messages: list[dict[str, str]],
        temperature: float = 0.7,
        max_tokens: int = 2048,
        **kwargs: Any,
    ) -> str:
        """Make a single (non-streaming) LLM call.

        Args:
            model: model identifier in litellm format (e.g. "claude-sonnet-4-6").
            messages: chat-format list of {"role": ..., "content": ...}.
            temperature: sampling temperature.
            max_tokens: maximum tokens to generate.
            **kwargs: forwarded to `litellm.completion()`.

        Returns:
            The response text (`response.choices[0].message.content`).
        """
        import litellm

        response = litellm.completion(
            model=model,
            messages=messages,
            temperature=temperature,
            max_tokens=max_tokens,
            **kwargs,
        )
        return response.choices[0].message.content or ""


# Module-level singleton instance. Import this to use the client.
llm_client: LLMClient = LLMClient()
