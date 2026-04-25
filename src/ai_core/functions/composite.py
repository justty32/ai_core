"""CompositeFunction — chains other registered functions together.

This is the S-expression idea in code: a function whose body is a sequence
of other function calls. Each output flows into the next input; the context
threads through all of them.

Closure:
    - func_chain: list of function IDs to call in order
    - registry: reference to the FunctionRegistry for ID lookup

Note on the registry reference:
    CompositeFunction needs to look up other functions at call time. We use
    dependency injection (the registry is passed at construction). On
    deserialization, the loader re-binds the registry — see
    `persistence/serializer.py`.
"""

from typing import TYPE_CHECKING, Any

from ai_core.functions.base import BaseFunction
from ai_core.types import Context, FunctionResult, Tokens

if TYPE_CHECKING:
    from ai_core.registry.registry import FunctionRegistry


class CompositeFunction(BaseFunction):
    """A function composed of other functions."""

    def __init__(
        self,
        func_id: str,
        func_chain: list[str],
        registry: "FunctionRegistry",
    ) -> None:
        """Construct a composite.

        Args:
            func_id: unique identifier
            func_chain: ordered list of function IDs to invoke
            registry: the registry to look up function IDs against
        """
        raise NotImplementedError

    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult:
        """Execute each function in the chain, threading tokens & context.

        Steps:
            1. current_tokens = tokens
            2. For each func_id in self.closure["func_chain"]:
                 func = self.closure["registry"].get(func_id)
                 current_tokens, context = func(current_tokens, context)
            3. Return (current_tokens, context).

        Errors:
            - If any func_id isn't registered, propagate FunctionNotFoundError.
        """
        raise NotImplementedError

    def to_dict(self) -> dict[str, Any]:
        """Serialize.

        The `registry` ref is NOT serialized — only `func_chain`.
        On load, the loader rebinds the registry.
        """
        raise NotImplementedError

    @classmethod
    def from_dict(
        cls,
        data: dict[str, Any],
        registry: "FunctionRegistry | None" = None,
    ) -> "CompositeFunction":
        """Reconstruct, binding the (passed-in) registry.

        Args:
            data: dict produced by to_dict()
            registry: registry to bind (the loader passes the freshly-built one)
        """
        raise NotImplementedError
