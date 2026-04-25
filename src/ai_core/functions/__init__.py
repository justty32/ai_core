"""Layer 1 — Function definitions.

A function in ai_core is a callable that maps (Tokens, Context) -> (Tokens, Context).
Each subclass of BaseFunction adapts a particular kind of computation:

    - LLMFunction: calls an LLM via LiteLLM
    - ShellFunction: runs an external shell script
    - CalculateFunction: wraps a Python callable
    - CompositeFunction: chains other functions together
"""

from ai_core.functions.base import BaseFunction
from ai_core.functions.calculate import CalculateFunction
from ai_core.functions.composite import CompositeFunction
from ai_core.functions.llm import LLMFunction
from ai_core.functions.shell import ShellFunction

__all__ = [
    "BaseFunction",
    "LLMFunction",
    "ShellFunction",
    "CalculateFunction",
    "CompositeFunction",
]
