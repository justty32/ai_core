"""CalculateFunction — wraps an arbitrary Python callable.

Closure:
    - func: a callable with signature (Tokens) -> Tokens

Limitation:
    NOT JSON-serializable. The persistence layer skips CalculateFunction
    instances on save() with a warning. Users must re-register them after load.
"""

from collections.abc import Callable
from typing import Any

from ai_core.functions.base import BaseFunction
from ai_core.types import Context, FunctionResult, Tokens


class CalculateFunction(BaseFunction):
    """Wraps a Python callable as an ai_core function."""

    def __init__(
        self,
        func_id: str,
        python_func: Callable[[Tokens], Tokens],
    ) -> None:
        """Construct a calculate function.

        Args:
            func_id: unique identifier
            python_func: callable that takes tokens (bytes) and returns tokens (bytes)
        """
        super().__init__(func_id)
        self.closure = {"func": python_func}
        self.metadata["type"] = "calculate"

    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult:
        """Apply the wrapped callable to tokens.

        Steps:
            1. Call self.closure["func"](tokens).
            2. If the result isn't bytes, encode str/repr to bytes.
            3. Return (result_bytes, context).
        """
        result = self.closure["func"](tokens)
        if isinstance(result, bytes):
            return result, context
        return str(result).encode(), context

    def to_dict(self) -> dict[str, Any]:
        """Raise NotImplementedError — Python callables can't be JSON-serialized.

        The persistence layer should detect this and skip with a warning.
        """
        raise NotImplementedError("CalculateFunction cannot be JSON-serialized")

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "CalculateFunction":
        """Raise NotImplementedError — see to_dict() rationale."""
        raise NotImplementedError("CalculateFunction cannot be JSON-serialized")
