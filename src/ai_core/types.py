"""Common type aliases used throughout ai_core.

These are pure aliases (no runtime cost). They exist to make function
signatures readable and to document the data contracts in one place.
"""

from typing import Any, TypeAlias

# Raw input/output of every function. Always bytes — tokenization is the LLM's job,
# and bytes support binary content (audio, images) without lossy conversion.
Tokens: TypeAlias = bytes

# Session-level global data. Owned by the caller (client), passed through the core,
# and returned (possibly mutated) to the caller. The core never persists it.
#
# Conventional keys (not enforced):
#   - "llm_history": list[dict[str, str]] — conversation history for LLM functions
#   - any other client-defined fields
Context: TypeAlias = dict[str, Any]

# Function descriptive metadata (resource profile, tags, semantic info).
# See ARCHITECTURE.md for the canonical structure.
Metadata: TypeAlias = dict[str, Any]

# Standard return type for any function: (new_tokens, updated_context).
FunctionResult: TypeAlias = tuple[Tokens, Context]
