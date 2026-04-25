"""BaseFunction — abstract base for all functions in ai_core.

A function has three components:

- **id**: unique identifier (string)
- **closure**: data owned by the function, fixed at construction
- **metadata**: descriptive info (tags, resource profile, etc.)

The function signature is always `(Tokens, Context) -> (Tokens, Context)`.
The function may read & mutate the context dict in-place, but should treat
its own closure as immutable.
"""

from abc import ABC, abstractmethod
from typing import Any

from ai_core.types import Context, FunctionResult, Tokens


def _default_metadata(func_id: str) -> dict[str, Any]:
    """Return a fresh default-metadata dict with reasonable empty values.

    Each call returns a NEW dict — never share refs across function instances.
    Subclasses should mutate selected fields after super().__init__().
    """
    return {
        "id": func_id,
        "type": None,
        "version": "1.0",
        "expanded_name": None,
        "tags": [],
        "grouping": "general",
        "conceptual_purpose": "",
        "domain": "general",
        "resource_profile": {
            "memory": {"self": 0, "running": 0, "peak": 0},
            "time": {"startup": 0, "actual_work": 0, "teardown": 0},
        },
        "input_requirements": {
            "source_type": None,
            "required_fields": [],
            "preconditions": [],
        },
        "output_produces": {},
    }


class BaseFunction(ABC):
    """Abstract base class for all functions.

    Subclasses must implement `__call__`. Concrete functions typically also
    populate the `closure` dict in `__init__` and override fields in
    `metadata`.

    Attributes:
        id: unique identifier
        closure: function-owned data (fixed at construction)
        metadata: descriptive info — see ARCHITECTURE.md for shape
    """

    id: str
    closure: dict[str, Any]
    metadata: dict[str, Any]

    def __init__(self, func_id: str) -> None:
        """Initialize with empty closure and default metadata.

        Subclasses should call `super().__init__(func_id)` first, then
        set `self.closure` and adjust `self.metadata`.
        """
        self.id = func_id
        self.closure = {}
        self.metadata = _default_metadata(func_id)

    @abstractmethod
    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult:
        """Execute the function.

        Args:
            tokens: input bytes
            context: session-level state (may be read & mutated)

        Returns:
            (output_tokens, updated_context)
        """
        ...

    def to_dict(self) -> dict[str, Any]:
        """Serialize to a JSON-compatible dict.

        Default shape:
            {
                "type": metadata["type"],
                "id": id,
                "closure": closure,
                "metadata": metadata,
            }

        Subclasses may override if their closure contains non-JSON values.
        """
        return {
            "type": self.metadata["type"],
            "id": self.id,
            "closure": self.closure,
            "metadata": self.metadata,
        }

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "BaseFunction":
        """Deserialize from a dict.

        Default impl raises NotImplementedError — concrete subclasses
        must implement this. The dispatching logic (mapping `type` to
        the right subclass) lives in `persistence/serializer.py`.
        """
        raise NotImplementedError
