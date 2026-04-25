"""ai_core — LISP-style functional composition for LLM calls.

Public API surface. Import everything users need from here:

    from ai_core import CoreService, LLMFunction, ShellFunction
"""

from ai_core.functions.base import BaseFunction
from ai_core.functions.calculate import CalculateFunction
from ai_core.functions.composite import CompositeFunction
from ai_core.functions.llm import LLMFunction
from ai_core.functions.shell import ShellFunction
from ai_core.registry.exceptions import (
    FunctionAlreadyExistsError,
    FunctionInUseError,
    FunctionNotFoundError,
    RegistryError,
)
from ai_core.registry.registry import FunctionRegistry
from ai_core.service.api import create_app
from ai_core.service.client import HTTPClient
from ai_core.service.core import CoreService
from ai_core.types import Context, FunctionResult, Metadata, Tokens

__version__ = "0.1.0"

__all__ = [
    "__version__",
    # types
    "Tokens",
    "Context",
    "Metadata",
    "FunctionResult",
    # functions
    "BaseFunction",
    "LLMFunction",
    "ShellFunction",
    "CalculateFunction",
    "CompositeFunction",
    # registry
    "FunctionRegistry",
    "RegistryError",
    "FunctionNotFoundError",
    "FunctionAlreadyExistsError",
    "FunctionInUseError",
    # service
    "CoreService",
    "create_app",
    "HTTPClient",
]
