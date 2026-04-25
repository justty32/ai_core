"""CoreService — the main entry point.

Owns:
    - FunctionRegistry (private)
    - reference to the global LLM client (singleton)

Does NOT own:
    - Sessions (clients manage them)
    - User context (passed in by callers, returned with mutations)

This is an in-process Python interface. The HTTP API in `api.py` wraps
a CoreService instance and exposes it over the network.
"""

from typing import Any

from ai_core.functions.base import BaseFunction
from ai_core.registry.registry import FunctionRegistry
from ai_core.types import Context, FunctionResult, Tokens


class CoreService:
    """In-process function execution service."""

    def __init__(self, registry: FunctionRegistry | None = None) -> None:
        """Build a service.

        Args:
            registry: an existing registry to use (mainly for testing).
                      If None, create a fresh one.
        """
        raise NotImplementedError

    # ---------- Function management ----------

    def register(self, func: BaseFunction) -> None:
        """Register a function. Forwards to self.registry.register()."""
        raise NotImplementedError

    def unregister(self, func_id: str) -> None:
        """Unregister a function. Forwards to self.registry.unregister()."""
        raise NotImplementedError

    def list_functions(self) -> list[str]:
        """List all registered function IDs."""
        raise NotImplementedError

    def get_function_info(self, func_id: str) -> dict[str, Any]:
        """Return a JSON-friendly dict describing a function.

        Returns:
            {
                "id": ...,
                "type": metadata["type"],
                "metadata": metadata,
            }

        Raises FunctionNotFoundError.
        """
        raise NotImplementedError

    # ---------- Execution ----------

    def execute(
        self,
        func_id: str,
        tokens: Tokens,
        context: Context | None = None,
    ) -> FunctionResult:
        """Look up the function and call it.

        Args:
            func_id: registered function ID
            tokens: input bytes
            context: caller-owned context (None -> empty dict)

        Returns:
            (output_tokens, context) — context may be mutated by the function

        Raises:
            FunctionNotFoundError if func_id isn't registered
            (any exception raised by the function)
        """
        raise NotImplementedError

    # ---------- Persistence ----------

    def save(self, path: str) -> None:
        """Save the registry to a JSON file. Delegates to persistence.serializer."""
        raise NotImplementedError

    @classmethod
    def load(cls, path: str) -> "CoreService":
        """Load a registry from a JSON file. Delegates to persistence.serializer."""
        raise NotImplementedError
