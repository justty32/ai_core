"""Common type definitions for ai_core."""

from typing import Any

# Raw input/output of every function. Always bytes — tokenization is the LLM's job,
# and bytes support binary content (audio, images) without lossy conversion.
type Tokens = bytes

# Session-level global data. Owned by the caller (client), passed through the core,
# and modified by functions.
#
# Common fields:
#   - "llm_history": list[dict[str, str]] — conversation history for LLM functions
#   - any other client-defined fields
type Context = dict[str, Any]

# Function descriptive metadata (resource profile, tags, semantic info).
# See ARCHITECTURE.md for the canonical structure.
type Metadata = dict[str, Any]

# Standard return type for any function: (new_tokens, updated_context).
type FunctionResult = tuple[Tokens, Context]
