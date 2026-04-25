# IMPLEMENTATION_PLAN — ai_core

**Audience**: implementation agents (other models). Read this start to finish.

**Goal**: turn the skeleton files (which all `raise NotImplementedError`) into a working implementation by replacing each `raise NotImplementedError` with real logic, then making all tests in `tests/` pass.

**Process**: do tasks in the listed order. Each task lists exactly which files to touch and which tests must pass after. Do **not** skip ahead — later tasks assume earlier ones work.

---

## Setup (do first, once)

```bash
cd /home/lorkhan/repo/ai_core
uv sync --extra dev
```

This installs all dependencies (litellm, fastapi, uvicorn, pydantic, httpx, pytest, ruff). After this, you can run `uv run pytest` and `uv run ruff check`.

If `uv sync` fails, stop and report — don't try to work around package errors by editing `pyproject.toml`.

---

## Conventions for every task

1. **Read** `ARCHITECTURE.md` once before starting. It's the source of truth for shapes & contracts.
2. **Read** the skeleton file before editing — its docstrings already describe the algorithm.
3. **Replace** every `raise NotImplementedError` with real code. Do not add extra methods or "helper" files unless the task says so.
4. **Run the tests listed in "Verify"** for the task. They must all pass before marking done.
5. **Don't** add comments restating what the code does. Don't write multi-paragraph docstrings either; the skeletons already have them.
6. **Don't** modify tests in `tests/` to make them pass. If a test seems wrong, stop and ask.
7. **Don't** edit `ARCHITECTURE.md`, `SYSTEM_DESIGN.md`, or this file.
8. **Type hints**: keep them. Don't widen types to `Any` to silence the type checker.

---

## Task order (do top to bottom)

### Phase A — Foundations (no dependencies)

#### Task A1 — `types.py`

Already complete (pure type aliases, no logic). Skip.

#### Task A2 — `registry/exceptions.py`

Already complete (empty exception classes). Skip.

#### Task A3 — `service/models.py`

Already complete (pure pydantic models). Skip.

#### Task A4 — `llm_client.py`

**File**: `src/ai_core/llm_client.py`

**Goal**: implement the singleton LiteLLM wrapper.

**Steps**:
1. `__new__` is already implemented (trivial singleton boilerplate). Don't touch it.
2. In `call`, call `litellm.completion(model=model, messages=messages, temperature=temperature, max_tokens=max_tokens, **kwargs)` and return `response.choices[0].message.content`.

**Verify**:
- The module imports without error: `python -c "from ai_core.llm_client import llm_client"`.
- `LLMClient()` returns the same instance every time.

(No dedicated tests — `LLMFunction` tests cover this via the `mock_llm_client` fixture.)

---

### Phase B — Function definitions (Layer 1)

#### Task B1 — `functions/base.py`

**File**: `src/ai_core/functions/base.py`

**Goal**: implement `BaseFunction.__init__` and serialization helpers.

**Steps**:
1. `__init__`: set `self.id = func_id`, `self.closure = {}`, `self.metadata = _default_metadata(func_id)`.
2. `to_dict`: return
   ```python
   {
       "type": self.metadata["type"],
       "id": self.id,
       "closure": self.closure,
       "metadata": self.metadata,
   }
   ```
   This default works for `LLMFunction` and `ShellFunction`. Subclasses with non-JSON closures override.
3. `from_dict`: this base impl should `raise NotImplementedError`. Concrete subclasses implement their own.

**Verify**: `uv run pytest tests/test_functions/test_base.py -v` passes.

---

#### Task B2 — `functions/llm.py`

**File**: `src/ai_core/functions/llm.py`

**Goal**: implement `LLMFunction` (call LLM, manage history in context).

**Steps**:
1. `__init__`: call `super().__init__(func_id)`. Set `self.closure = {"model": model, "system_prompt": system_prompt, "temperature": temperature, "max_tokens": max_tokens}`. Set `self.metadata["type"] = "llm"`.
2. `__call__`:
   - `history = list(context.get("llm_history", []))` (copy so we don't accidentally mutate caller's list before we're sure).
   - If `self.closure["system_prompt"]` is non-empty AND `history` is empty: prepend `{"role": "system", "content": self.closure["system_prompt"]}`.
   - Append `{"role": "user", "content": tokens.decode()}`.
   - Import `from ai_core.llm_client import llm_client` and call:
     ```python
     reply = llm_client.call(
         model=self.closure["model"],
         messages=history,
         temperature=self.closure["temperature"],
         max_tokens=self.closure["max_tokens"],
     )
     ```
   - Append `{"role": "assistant", "content": reply}`.
   - `context["llm_history"] = history`.
   - Return `(reply.encode(), context)`.
3. `to_dict`: use the base impl (delete the override or call `super().to_dict()`).
4. `from_dict`: extract `closure["model"]`, `closure["system_prompt"]`, `closure["temperature"]`, `closure["max_tokens"]`. Construct an `LLMFunction` with them. Then overwrite `metadata` with `data["metadata"]`. Return it.

**Important**: `mock_llm_client` in `conftest.py` patches `ai_core.llm_client.llm_client`. If you `from ai_core.llm_client import llm_client` at **module top**, the patch won't take effect (you'll have a stale reference). **Import inside `__call__`** so the patched module attribute is read each call.

**Verify**: `uv run pytest tests/test_functions/test_llm.py -v` passes.

---

#### Task B3 — `functions/shell.py`

**File**: `src/ai_core/functions/shell.py`

**Goal**: implement `ShellFunction` (run a script via subprocess).

**Steps**:
1. `__init__`: call super. Set `self.closure = {"script_path": script_path, "timeout": timeout}`. Set `self.metadata["type"] = "shell"`.
2. `__call__`:
   - `import subprocess` (at module top is fine).
   - Run:
     ```python
     result = subprocess.run(
         ["bash", self.closure["script_path"], tokens.decode()],
         capture_output=True,
         timeout=self.closure["timeout"],
     )
     ```
   - If `result.returncode != 0`: `raise RuntimeError(f"Shell function {self.id} failed: {result.stderr.decode()}")`.
   - Return `(result.stdout, context)`.
   - **Don't** wrap `subprocess.TimeoutExpired` — let it propagate (the test asserts on it).
3. `to_dict`: use base impl.
4. `from_dict`: extract `closure["script_path"]` and `closure["timeout"]`. Construct. Overwrite metadata. Return.

**Verify**: `uv run pytest tests/test_functions/test_shell.py -v` passes.

---

#### Task B4 — `functions/calculate.py`

**File**: `src/ai_core/functions/calculate.py`

**Goal**: implement `CalculateFunction` (wrap a Python callable).

**Steps**:
1. `__init__`: super. `self.closure = {"func": python_func}`. `self.metadata["type"] = "calculate"`.
2. `__call__`:
   - `result = self.closure["func"](tokens)`.
   - If `isinstance(result, bytes)`: return `(result, context)`.
   - Else: return `(str(result).encode(), context)`.
3. `to_dict`: `raise NotImplementedError("CalculateFunction cannot be JSON-serialized")`.
4. `from_dict`: same — raise.

**Verify**: `uv run pytest tests/test_functions/test_calculate.py -v` passes.

---

### Phase C — Registry (Layer 2)

#### Task C1 — `registry/registry.py`

**File**: `src/ai_core/registry/registry.py`

**Goal**: implement `FunctionRegistry`.

**Steps**:
1. `register(func)`:
   - If `func.id in self._functions`: raise `FunctionAlreadyExistsError(f"Function {func.id} already registered")`.
   - Store: `self._functions[func.id] = func`.
   - Type index: `self._by_type.setdefault(func.metadata["type"], []).append(func.id)` (skip if `type` is None).
   - Tag index: for each `tag` in `func.metadata["tags"]`: `self._by_tag.setdefault(tag, []).append(func.id)`.
   - If `func.metadata["type"] == "composite"`: for each `ref_id` in `func.closure["func_chain"]`: `self._references.setdefault(ref_id, set()).add(func.id)`.
2. `unregister(func_id)`:
   - If not in `_functions`: raise `FunctionNotFoundError`.
   - If `self._references.get(func_id)`: raise `FunctionInUseError(f"{func_id} is still referenced by {sorted(self._references[func_id])}")`.
   - Pop from `_functions`. Remove from `_by_type` and `_by_tag` (filter the lists).
   - If the removed function was a composite, remove its ID from each `_references[ref]` set in its chain. Drop empty sets.
3. `get(func_id)`: return from `_functions`, raise `FunctionNotFoundError` if missing.
4. `has(func_id)`: `return func_id in self._functions`.
5. `list_all()`: `return sorted(self._functions.keys())`.
6. `find_by_type(func_type)`: `return [self._functions[fid] for fid in self._by_type.get(func_type, [])]`.
7. `find_by_tag(tag)`: same pattern with `_by_tag`.
8. `get_references(func_id)`: `return set(self._references.get(func_id, set()))` (return a copy).

**Verify**: `uv run pytest tests/test_registry/ -v` passes.

---

#### Task C2 — `functions/composite.py`

**File**: `src/ai_core/functions/composite.py`

**Goal**: implement `CompositeFunction`. Comes after the registry because tests call `registry.register()` on composites.

**Steps**:
1. `__init__`: super. `self.closure = {"func_chain": list(func_chain), "registry": registry}`. `self.metadata["type"] = "composite"`.
2. `__call__`:
   - `current = tokens`.
   - For each `fid` in `self.closure["func_chain"]`:
     - `func = self.closure["registry"].get(fid)` (will raise `FunctionNotFoundError` if missing).
     - `current, context = func(current, context)`.
   - Return `(current, context)`.
3. `to_dict`: override base. The registry isn't serializable, so:
   ```python
   return {
       "type": "composite",
       "id": self.id,
       "closure": {"func_chain": list(self.closure["func_chain"])},
       "metadata": self.metadata,
   }
   ```
4. `from_dict(data, registry=None)`:
   - If `registry is None`: raise `ValueError("CompositeFunction.from_dict requires a registry")`.
   - Construct a `CompositeFunction(data["id"], data["closure"]["func_chain"], registry)`.
   - Overwrite `metadata = data["metadata"]`.
   - Return it.

**Verify**: `uv run pytest tests/test_functions/test_composite.py tests/test_registry/ -v` passes (re-run registry tests to catch regressions).

---

### Phase D — Core service (Layer 3)

#### Task D1 — `service/core.py`

**File**: `src/ai_core/service/core.py`

**Goal**: implement `CoreService`.

**Steps**:
1. `__init__(registry=None)`:
   - `self.registry = registry if registry is not None else FunctionRegistry()`.
2. `register(func)`: `self.registry.register(func)`.
3. `unregister(func_id)`: `self.registry.unregister(func_id)`.
4. `list_functions()`: `return self.registry.list_all()`.
5. `get_function_info(func_id)`:
   - `func = self.registry.get(func_id)` (raises if missing).
   - Return `{"id": func.id, "type": func.metadata["type"], "metadata": func.metadata}`.
6. `execute(func_id, tokens, context=None)`:
   - If `context is None`: `context = {}`.
   - `func = self.registry.get(func_id)` (raises if missing).
   - Return `func(tokens, context)`.
7. `save(path)`:
   - `from ai_core.persistence.serializer import save_core; save_core(self, path)`.
   - (Local import to avoid circular import at module load.)
8. `load(path)` (classmethod):
   - `from ai_core.persistence.serializer import load_core; return load_core(path)`.

**Verify**: `uv run pytest tests/test_service/test_core.py -v` passes.

---

#### Task D2 — `service/api.py`

**File**: `src/ai_core/service/api.py`

**Goal**: implement the FastAPI app factory.

**Steps**:
1. Import: `from fastapi import FastAPI, HTTPException, Request`, `from fastapi.responses import JSONResponse`, and the relevant pydantic models, exceptions, base64.
2. In `create_app(core)`:
   ```python
   app = FastAPI(title="ai_core", version="0.1.0")

   @app.exception_handler(FunctionNotFoundError)
   async def _not_found_handler(request: Request, exc: FunctionNotFoundError):
       return JSONResponse(
           status_code=404,
           content={"error": "function_not_found", "detail": str(exc)},
       )

   @app.post("/execute", response_model=ExecuteResponse)
   def execute_endpoint(req: ExecuteRequest) -> ExecuteResponse:
       tokens = base64.b64decode(req.tokens.encode())
       out_tokens, out_context = core.execute(req.func_id, tokens, req.context)
       return ExecuteResponse(
           tokens=base64.b64encode(out_tokens).decode(),
           context=out_context,
       )

   @app.get("/functions", response_model=list[str])
   def list_endpoint() -> list[str]:
       return core.list_functions()

   @app.get("/functions/{func_id}", response_model=FunctionInfo)
   def info_endpoint(func_id: str) -> FunctionInfo:
       info = core.get_function_info(func_id)
       return FunctionInfo(**info)

   return app
   ```
3. Imports of `FunctionNotFoundError`: `from ai_core.registry.exceptions import FunctionNotFoundError`.

**Verify**: `uv run pytest tests/test_service/test_api.py -v` passes.

---

#### Task D3 — `service/client.py`

**File**: `src/ai_core/service/client.py`

**Goal**: implement `HTTPClient`.

**Steps**:
1. `__init__(base_url, timeout)`:
   - `self.base_url = base_url.rstrip("/")`.
   - `self._client = httpx.Client(base_url=self.base_url, timeout=timeout)`.
2. `execute(func_id, tokens, context=None)`:
   - `import base64`.
   - `payload = {"func_id": func_id, "tokens": base64.b64encode(tokens).decode(), "context": context or {}}`.
   - `resp = self._client.post("/execute", json=payload)`.
   - `resp.raise_for_status()`.
   - `body = resp.json()`.
   - `return base64.b64decode(body["tokens"].encode()), body["context"]`.
3. `list_functions()`: `resp = self._client.get("/functions"); resp.raise_for_status(); return resp.json()`.
4. `get_function_info(func_id)`: same pattern with `/functions/{func_id}`.

**Note for test compatibility**: `tests/test_service/test_client.py` directly assigns `client._client = httpx.Client(...)` after construction. Make sure `_client` is the attribute name you use.

**Verify**: `uv run pytest tests/test_service/test_client.py -v` passes.

---

### Phase E — Persistence

#### Task E1 — `persistence/serializer.py`

**File**: `src/ai_core/persistence/serializer.py`

**Goal**: implement `save_core` and `load_core`.

**Steps**:

1. `save_core(core, path)`:
   ```python
   import json, logging
   from ai_core.functions.calculate import CalculateFunction

   logger = logging.getLogger(__name__)

   functions_data = []
   for func_id in core.list_functions():
       func = core.registry.get(func_id)
       if isinstance(func, CalculateFunction):
           logger.warning("Skipping calculate function %s during save", func_id)
           continue
       functions_data.append(func.to_dict())

   payload = {"version": SAVE_FORMAT_VERSION, "functions": functions_data}
   with open(path, "w") as f:
       json.dump(payload, f, indent=2)
   ```

2. `load_core(path)`:
   ```python
   import json, logging
   from ai_core.functions.composite import CompositeFunction
   from ai_core.functions.llm import LLMFunction
   from ai_core.functions.shell import ShellFunction
   from ai_core.service.core import CoreService

   logger = logging.getLogger(__name__)

   with open(path) as f:
       data = json.load(f)

   if data.get("version") != SAVE_FORMAT_VERSION:
       raise ValueError(
           f"Unsupported save format version: {data.get('version')}; "
           f"expected {SAVE_FORMAT_VERSION}"
       )

   core = CoreService()

   # First pass: non-composites
   type_dispatch = {
       "llm": LLMFunction.from_dict,
       "shell": ShellFunction.from_dict,
   }
   composite_entries = []
   for entry in data["functions"]:
       t = entry["type"]
       if t == "composite":
           composite_entries.append(entry)
       elif t == "calculate":
           logger.warning("Cannot restore calculate function %s", entry.get("id"))
       elif t in type_dispatch:
           core.register(type_dispatch[t](entry))
       else:
           raise ValueError(f"Unknown function type: {t}")

   # Second pass: composites (need registry)
   for entry in composite_entries:
       core.register(CompositeFunction.from_dict(entry, registry=core.registry))

   return core
   ```

**Verify**:
- `uv run pytest tests/test_persistence/ -v` passes.
- The full test suite passes: `uv run pytest -v`.

---

### Phase F — Final integration

#### Task F1 — Run all tests

```bash
uv run pytest -v
```

All tests must pass. If any fail:
- Read the failing test carefully.
- Check the implementation against the task description above.
- Don't modify the test.
- If the test seems incorrect after careful reading, **stop** and report the issue.

#### Task F2 — Run examples

```bash
uv run python examples/01_basic_usage.py
uv run python examples/02_composite.py
```

Expected outputs:

- 01: `output : HELLO WORLD`, `context: {}`, `functions: ['upper_v1']`.
- 02: `output: [OLLEH]`.

(Examples 03 and 04 require running a server / having an LLM key; they are smoke tests for human review.)

#### Task F3 — Lint check

```bash
uv run ruff check src/ai_core/
```

Should pass without errors. Fix any genuine style issues; `# noqa` comments are not allowed.

---

## Summary checklist

- [ ] `uv sync --extra dev` succeeded
- [ ] B1 — base.py
- [ ] B2 — llm.py
- [ ] B3 — shell.py
- [ ] B4 — calculate.py
- [ ] A4 — llm_client.py
- [ ] C1 — registry.py
- [ ] C2 — composite.py
- [ ] D1 — core.py
- [ ] D2 — api.py
- [ ] D3 — client.py
- [ ] E1 — serializer.py
- [ ] F1 — all tests pass
- [ ] F2 — examples 01 & 02 produce expected output
- [ ] F3 — `ruff check` clean

When all boxes are ticked, the implementation is done. Commit and stop.

---

## Out of scope (do not implement)

These are explicitly NOT part of this implementation. If a test or example depends on them, that's a bug — report it.

- Streaming LLM responses
- Async function execution
- API authentication
- Function versioning / namespaces beyond simple IDs
- Resource accounting / scheduling
- Multi-process / distributed execution
- A CLI tool

These are all listed in `ARCHITECTURE.md` § "Open Questions" and will be tackled in a future phase.
