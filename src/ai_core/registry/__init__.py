"""Layer 2 — Function management.

The FunctionRegistry stores all registered functions, indexes them by type
and tag, and tracks reference relationships (so a function being used by
a composite cannot be silently removed).
"""

from ai_core.registry.exceptions import (
    FunctionAlreadyExistsError,
    FunctionInUseError,
    FunctionNotFoundError,
    RegistryError,
)
from ai_core.registry.registry import FunctionRegistry

__all__ = [
    "FunctionRegistry",
    "RegistryError",
    "FunctionNotFoundError",
    "FunctionAlreadyExistsError",
    "FunctionInUseError",
]
