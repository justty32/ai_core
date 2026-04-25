# ARCHITECTURE — ai_core

**Last updated**: 2026-04-25
**Status**: Implementation-ready architecture spec

This document defines the **concrete architecture** for implementing ai_core based on `SYSTEM_DESIGN.md`. It covers:

1. Project layout (directories, files, modules)
2. Module responsibilities and class signatures
3. Data flow through the system
4. Key design decisions
5. Dependency choices and rationale

---

## Tech Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Language | Python 3.12+ | Modern type syntax, latest features |
| Package manager | uv | Fast, modern, what user runs |
| LLM client | LiteLLM | Unified interface to all providers |
| Web framework | FastAPI | Type-safe, async, auto-docs (OpenAPI) |
| ASGI server | uvicorn | FastAPI standard runtime |
| Data models | pydantic v2 | FastAPI native, runtime validation |
| Testing | pytest | Industry standard |
| Serialization | JSON | Human-readable, simple |

---

## Project Layout

```
ai_core/
├── pyproject.toml                # uv project config
├── README.md
├── CLAUDE.md                     # Project guidance for Claude Code
├── SYSTEM_DESIGN.md              # Main design doc
├── SYSTEM_DESIGN_EXTRA.md        # Design conclusion
├── ARCHITECTURE.md               # This file
├── IMPLEMENTATION_PLAN.md        # Atomic tasks for implementers
├── old/                          # Previous design iterations
│
├── src/
│   └── ai_core/
│       ├── __init__.py           # Public API surface
│       ├── types.py              # Common type aliases
│       ├── llm_client.py         # LiteLLM singleton wrapper
│       ├── functions/            # Layer 1: Function definitions
│       │   ├── __init__.py
│       │   ├── base.py           # BaseFunction (abstract)
│       │   ├── llm.py            # LLMFunction
│       │   ├── shell.py          # ShellFunction
│       │   ├── calculate.py      # CalculateFunction
│       │   └── composite.py      # CompositeFunction
│       ├── registry/             # Layer 2: Function management
│       │   ├── __init__.py
│       │   ├── exceptions.py     # Custom exceptions
│       │   └── registry.py       # FunctionRegistry
│       ├── service/              # Layer 3: Client-server interface
│       │   ├── __init__.py
│       │   ├── core.py           # CoreService (the main entry point)
│       │   ├── models.py         # pydantic request/response models
│       │   ├── api.py            # FastAPI app factory
│       │   └── client.py         # HTTPClient (Python-side wrapper)
│       └── persistence/          # Core save/load
│           ├── __init__.py
│           └── serializer.py     # JSON serialization
│
├── tests/
│   ├── __init__.py
│   ├── conftest.py               # Shared pytest fixtures
│   ├── test_functions/
│   │   ├── test_base.py
│   │   ├── test_llm.py
│   │   ├── test_shell.py
│   │   ├── test_calculate.py
│   │   └── test_composite.py
│   ├── test_registry/
│   │   └── test_registry.py
│   ├── test_service/
│   │   ├── test_core.py
│   │   ├── test_api.py
│   │   └── test_client.py
│   └── test_persistence/
│       └── test_serializer.py
│
├── examples/
│   ├── 01_basic_usage.py         # Register a function, call it
│   ├── 02_composite.py           # Compose functions
│   ├── 03_run_server.py          # Start FastAPI server
│   └── 04_save_load_core.py      # Persist and restore core
│
└── scripts/
    └── shell/
        └── echo.sh               # Example shell function
```

---

## Type System (`types.py`)

Common type aliases used throughout the codebase.

```python
from typing import Any, TypeAlias

# Raw input/output of every function
Tokens: TypeAlias = bytes

# Session-level global data, owned by the caller (client)
Context: TypeAlias = dict[str, Any]

# Function descriptive metadata (resource profile, tags, etc.)
Metadata: TypeAlias = dict[str, Any]

# Function output: (new_tokens, updated_context)
FunctionResult: TypeAlias = tuple[Tokens, Context]
```

---

## Layer 1: Function Definitions

### `functions/base.py` — BaseFunction

Abstract base class for all function types.

```python
from abc import ABC, abstractmethod
from ai_core.types import Tokens, Context, FunctionResult

class BaseFunction(ABC):
    """
    Base class for all functions.

    Each function has three components:
    - id: unique identifier
    - closure: data owned by the function (fixed at construction)
    - metadata: descriptive info about the function
    """

    id: str
    closure: dict[str, Any]
    metadata: dict[str, Any]

    def __init__(self, func_id: str) -> None:
        """Initialize with empty closure and default metadata."""
        ...

    @abstractmethod
    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult:
        """
        Execute the function.

        Args:
            tokens: input bytes
            context: session-level global state (can be read & modified)

        Returns:
            (output_tokens, updated_context)
        """
        ...

    def to_dict(self) -> dict[str, Any]:
        """Serialize to a JSON-compatible dict for persistence."""
        ...

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "BaseFunction":
        """Deserialize from a dict. Override in subclasses if needed."""
        ...
```

**Default metadata structure** (set in `__init__`):

```python
{
    "id": func_id,
    "type": None,              # filled by subclass
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
```

### `functions/llm.py` — LLMFunction

Calls an LLM via the global LiteLLM client.

```python
class LLMFunction(BaseFunction):
    """
    LLM-calling function.

    Closure (fixed at construction):
        model: e.g. "claude-sonnet-4-6"
        system_prompt: prepended to messages
        temperature, max_tokens

    Reads from context:
        context["llm_history"]: list of {role, content}

    Writes to context:
        context["llm_history"]: appended with new turn
    """

    def __init__(
        self,
        func_id: str,
        model: str,
        system_prompt: str = "",
        temperature: float = 0.7,
        max_tokens: int = 2048,
    ) -> None: ...

    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult: ...
```

### `functions/shell.py` — ShellFunction

Runs an external shell script.

```python
class ShellFunction(BaseFunction):
    """
    Execute a shell script via subprocess.

    Closure:
        script_path: path to the executable script
        timeout: max runtime in seconds

    Behavior:
        Pipes tokens.decode() as the first arg to the script.
        Captures stdout as the result.
        Does not modify context.
    """

    def __init__(
        self,
        func_id: str,
        script_path: str,
        timeout: int = 30,
    ) -> None: ...

    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult: ...
```

### `functions/calculate.py` — CalculateFunction

Wraps an arbitrary Python callable.

```python
class CalculateFunction(BaseFunction):
    """
    Wraps a Python function.

    Closure:
        func: the wrapped callable, signature (Tokens) -> Tokens

    Limitation:
        NOT JSON-serializable (the callable is a Python object).
        Persistence will skip CalculateFunction instances in v1.
    """

    def __init__(
        self,
        func_id: str,
        python_func: Callable[[Tokens], Tokens],
    ) -> None: ...

    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult: ...
```

### `functions/composite.py` — CompositeFunction

Combines other registered functions into a chain (S-expression style).

```python
class CompositeFunction(BaseFunction):
    """
    Chains other functions together.

    Closure:
        func_chain: list of function IDs to execute in order
        registry: ref to FunctionRegistry for ID lookup

    Execution:
        Each function's output becomes the next's input.
        Context flows through all functions, accumulating mutations.
    """

    def __init__(
        self,
        func_id: str,
        func_chain: list[str],
        registry: "FunctionRegistry",  # forward reference
    ) -> None: ...

    def __call__(self, tokens: Tokens, context: Context) -> FunctionResult: ...
```

**Note on `registry` dependency**: CompositeFunction needs to look up other functions by ID. We use **dependency injection** — the registry is passed at construction. On deserialization, the loader rebinds the registry.

---

## Layer 2: Function Management

### `registry/exceptions.py`

```python
class RegistryError(Exception):
    """Base class for registry exceptions."""

class FunctionNotFoundError(RegistryError):
    """Raised when a function ID is not registered."""

class FunctionAlreadyExistsError(RegistryError):
    """Raised when trying to register a duplicate ID."""

class FunctionInUseError(RegistryError):
    """Raised when trying to unregister a function still referenced by others."""
```

### `registry/registry.py` — FunctionRegistry

```python
from ai_core.functions.base import BaseFunction

class FunctionRegistry:
    """
    Manages all registered functions.

    Responsibilities:
        - register / unregister functions
        - lookup by ID, type, tag
        - track references (composite -> components)
        - prevent removal of in-use functions
    """

    def __init__(self) -> None:
        self._functions: dict[str, BaseFunction] = {}
        self._references: dict[str, set[str]] = {}  # func_id -> {referrers}
        self._by_type: dict[str, list[str]] = {}    # type -> [func_ids]
        self._by_tag: dict[str, list[str]] = {}     # tag -> [func_ids]

    # ---- mutation ----
    def register(self, func: BaseFunction) -> None:
        """Register a function and update indexes. Raises FunctionAlreadyExistsError."""
        ...

    def unregister(self, func_id: str) -> None:
        """Remove a function. Raises FunctionInUseError if still referenced."""
        ...

    # ---- lookup ----
    def get(self, func_id: str) -> BaseFunction:
        """Get a function by ID. Raises FunctionNotFoundError."""
        ...

    def has(self, func_id: str) -> bool:
        """Check if a function exists."""
        ...

    def list_all(self) -> list[str]:
        """Return all registered function IDs."""
        ...

    def find_by_type(self, func_type: str) -> list[BaseFunction]:
        """Find functions by their `type` metadata field."""
        ...

    def find_by_tag(self, tag: str) -> list[BaseFunction]:
        """Find functions by tag."""
        ...

    # ---- references ----
    def get_references(self, func_id: str) -> set[str]:
        """Return the set of function IDs that reference `func_id`."""
        ...
```

---

## Layer 3: Client-Server Interface

### `service/models.py` — pydantic request/response models

```python
from pydantic import BaseModel, Field

class ExecuteRequest(BaseModel):
    func_id: str
    tokens: str = Field(description="base64-encoded bytes")
    context: dict[str, Any] = Field(default_factory=dict)

class ExecuteResponse(BaseModel):
    tokens: str = Field(description="base64-encoded bytes")
    context: dict[str, Any]

class FunctionInfo(BaseModel):
    id: str
    type: str
    metadata: dict[str, Any]

class ErrorResponse(BaseModel):
    error: str
    detail: str | None = None
```

### `service/core.py` — CoreService

The main entry point. Owns the registry and exposes execution.

```python
class CoreService:
    """
    Stateless core service.

    Owns:
        - FunctionRegistry
        - Reference to global LLM client (singleton)

    Does NOT own:
        - Sessions (clients manage their own)
        - User context (passed in by callers)
    """

    def __init__(self, registry: FunctionRegistry | None = None) -> None: ...

    # ---- function management ----
    def register(self, func: BaseFunction) -> None: ...
    def unregister(self, func_id: str) -> None: ...
    def list_functions(self) -> list[str]: ...
    def get_function_info(self, func_id: str) -> dict[str, Any]: ...

    # ---- execution ----
    def execute(
        self,
        func_id: str,
        tokens: Tokens,
        context: Context | None = None,
    ) -> FunctionResult:
        """The unified execute interface."""
        ...

    # ---- persistence ----
    def save(self, path: str) -> None:
        """Serialize core to JSON file."""
        ...

    @classmethod
    def load(cls, path: str) -> "CoreService":
        """Deserialize core from JSON file."""
        ...
```

### `service/api.py` — FastAPI app

```python
from fastapi import FastAPI, HTTPException
from ai_core.service.core import CoreService

def create_app(core: CoreService) -> FastAPI:
    """Factory: build a FastAPI app bound to a given CoreService."""
    app = FastAPI(title="ai_core", version="0.1.0")

    @app.post("/execute", response_model=ExecuteResponse)
    def execute(req: ExecuteRequest) -> ExecuteResponse: ...

    @app.get("/functions", response_model=list[str])
    def list_functions() -> list[str]: ...

    @app.get("/functions/{func_id}", response_model=FunctionInfo)
    def get_function(func_id: str) -> FunctionInfo: ...

    return app
```

### `service/client.py` — HTTPClient

Python wrapper for HTTP calls (so users don't need to handle base64 themselves).

```python
class HTTPClient:
    """Python-side HTTP wrapper for the Core API."""

    def __init__(self, base_url: str = "http://localhost:8000") -> None: ...

    def execute(
        self,
        func_id: str,
        tokens: bytes,
        context: dict[str, Any] | None = None,
    ) -> tuple[bytes, dict[str, Any]]: ...

    def list_functions(self) -> list[str]: ...
    def get_function_info(self, func_id: str) -> dict[str, Any]: ...
```

---

## Supporting Modules

### `llm_client.py` — LLM singleton

```python
import litellm

class LLMClient:
    """Singleton wrapper around LiteLLM."""

    _instance: "LLMClient | None" = None

    def __new__(cls) -> "LLMClient": ...

    def call(
        self,
        model: str,
        messages: list[dict[str, str]],
        temperature: float = 0.7,
        max_tokens: int = 2048,
        **kwargs: Any,
    ) -> str:
        """Make an LLM call. Returns the response content text."""
        ...

# Module-level singleton instance
llm_client = LLMClient()
```

### `persistence/serializer.py` — Save/Load

```python
def save_core(core: CoreService, path: str) -> None:
    """
    Serialize a CoreService to JSON.

    Format:
        {
            "version": "0.1.0",
            "functions": [
                {"type": "llm", "id": "...", "closure": {...}, "metadata": {...}},
                {"type": "shell", "id": "...", "closure": {...}, ...},
                {"type": "composite", "id": "...", "closure": {"func_chain": [...]}, ...},
            ]
        }

    Limitations:
        - CalculateFunction is NOT serialized (skipped with warning).
        - Closure data must be JSON-compatible.
    """
    ...

def load_core(path: str) -> CoreService:
    """
    Deserialize a CoreService from JSON.

    The loader resolves function types and rebuilds the registry.
    Composite functions get a fresh registry reference.
    """
    ...
```

---

## Data Flow

### Request flow (HTTP API path)

```
┌──────────────┐
│ Client       │
│  context = { │
│    history,  │
│    vars,     │
│  }           │
└──────┬───────┘
       │ POST /execute
       │ { func_id, tokens(b64), context }
       ▼
┌──────────────┐
│ FastAPI app  │  decode base64 → bytes
│   api.py     │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ CoreService  │
│  .execute()  │
└──────┬───────┘
       │ registry.get(func_id)
       ▼
┌──────────────┐
│ BaseFunction │  → reads/writes context
│  __call__    │  → calls LLM/shell/etc
└──────┬───────┘
       │ (output_tokens, updated_context)
       ▼
┌──────────────┐
│ FastAPI app  │  encode bytes → base64
└──────┬───────┘
       │ Response: { tokens(b64), context }
       ▼
   Client (saves new context for next request)
```

### Key invariant

**The client owns the context.** The Core never persists context between requests — it's stateless w.r.t. sessions. This is what makes `Core` truly portable and lets clients run independently (CLI, web, mobile, etc.).

---

## Key Design Decisions

### 1. Tokens are `bytes`, not `str`

Why: aligns with "raw, pre-tokenizer text" philosophy. Supports binary content (audio, files). Tokenization is the LLM's job.

Trade-off: clients must encode/decode at the API boundary (we use base64 in JSON).

### 2. Context is `dict`, not a typed model

Why: dynamic extensibility. Functions can add fields (`denoised: True`, `confidence: 0.95`) without schema changes. Aligns with LISP's "data and code are the same" philosophy.

Trade-off: no compile-time validation. Mitigation: convention + tests.

### 3. CompositeFunction holds a registry reference

Why: composites need to look up chained functions by ID. Alternative would be a global registry, but explicit DI is cleaner.

Trade-off: serialization needs to rebind the registry on load.

### 4. CalculateFunction is NOT serializable

Why: arbitrary Python callables can't safely round-trip through JSON. Pickling them is fragile and unsafe.

Decision for v1: skip them on save with a warning. Users can re-register after load.

### 5. Singleton LLM client

Why: one connection pool, one place to configure providers, simple to mock in tests.

Trade-off: harder to test in isolation. Mitigation: pytest fixture that monkeypatches the singleton.

### 6. FastAPI factory pattern

Why: `create_app(core)` lets us build apps for different cores (e.g. testing). Avoids module-level state.

### 7. src/ layout

Why: standard modern Python convention. Forces tests to import from the installed package, not the source dir, catching packaging bugs early.

---

## Public API Surface

`src/ai_core/__init__.py` exports:

```python
from ai_core.types import Tokens, Context, Metadata, FunctionResult
from ai_core.functions.base import BaseFunction
from ai_core.functions.llm import LLMFunction
from ai_core.functions.shell import ShellFunction
from ai_core.functions.calculate import CalculateFunction
from ai_core.functions.composite import CompositeFunction
from ai_core.registry.registry import FunctionRegistry
from ai_core.registry.exceptions import (
    RegistryError,
    FunctionNotFoundError,
    FunctionAlreadyExistsError,
    FunctionInUseError,
)
from ai_core.service.core import CoreService
from ai_core.service.api import create_app
from ai_core.service.client import HTTPClient

__all__ = [
    "Tokens", "Context", "Metadata", "FunctionResult",
    "BaseFunction", "LLMFunction", "ShellFunction",
    "CalculateFunction", "CompositeFunction",
    "FunctionRegistry",
    "RegistryError", "FunctionNotFoundError",
    "FunctionAlreadyExistsError", "FunctionInUseError",
    "CoreService", "create_app", "HTTPClient",
]
```

Typical usage:

```python
from ai_core import CoreService, LLMFunction, ShellFunction, CompositeFunction

core = CoreService()
core.register(LLMFunction("chat_v1", model="claude-sonnet-4-6"))
output, context = core.execute("chat_v1", b"hello", context={})
```

---

## Implementation Order

See `IMPLEMENTATION_PLAN.md` for the atomic task list. High-level phases:

1. **Setup**: `pyproject.toml`, directory structure, `__init__.py` files
2. **Types**: `types.py`
3. **Exceptions**: `registry/exceptions.py`
4. **LLM client**: `llm_client.py`
5. **Function base**: `functions/base.py`
6. **Concrete functions**: `llm.py`, `shell.py`, `calculate.py`
7. **Registry**: `registry/registry.py`
8. **Composite function**: `functions/composite.py` (depends on registry)
9. **Core service**: `service/core.py`
10. **API**: `service/models.py`, `service/api.py`
11. **HTTP client**: `service/client.py`
12. **Persistence**: `persistence/serializer.py`
13. **Examples & integration tests**

Each step has its own task with acceptance criteria in `IMPLEMENTATION_PLAN.md`.

---

## Open Questions (to revisit later)

1. **Streaming LLM responses** — current design returns full text. Streaming would require changing the function signature.
2. **Async execution** — all functions are sync. May need async variant for concurrent requests.
3. **Authentication** — API has no auth. Add when needed.
4. **Function versioning** — multiple versions of the same function (`llm/claude_v1`, `v2`). Defer namespace design.
5. **Resource accounting** — `metadata.resource_profile` is descriptive only; not enforced. Defer scheduling.

These are out of scope for v1.
