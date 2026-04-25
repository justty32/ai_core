# for_agent.md

**Audience**: AI agents (any model) brought in to work on this repo.

You are not the first agent to touch this project. Read this whole file before doing anything else. It will save you from making changes the user has already rejected.

---

## TL;DR — current state

This is a **skeleton** project. The architecture is decided. Most files exist with type-hinted signatures and docstrings, but bodies are `raise NotImplementedError`.

You are probably here for one of three reasons:

1. **Implement the skeleton** → go straight to [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) and do exactly what it says.
2. **Improve the design** → read [`ARCHITECTURE.md`](ARCHITECTURE.md), then this file's "Open questions" section.
3. **Fix a bug or extend a feature** → read the relevant module's source, then check this file's "Conventions" before changing things.

If you're not sure which applies, ask the user.

---

## Reading order

Don't skip steps. Each builds on the previous.

1. **`CLAUDE.md`** — 30 sec — project vision and the user's preferences.
2. **`SYSTEM_DESIGN.md`** — 5 min — the high-level three-layer model. **This is the source of truth for the philosophy.** Don't argue with it; if you disagree, raise it as a question, don't refactor.
3. **`ARCHITECTURE.md`** — 10 min — concrete shapes: directory layout, class signatures, data flow, key design decisions and their *rationale* (the "why" matters).
4. **`SYSTEM_DESIGN_EXTRA.md`** — optional, 3 min — a summarized conclusion of the design conversation.
5. **`old/`** — only if you're confused about why something is the way it is. These are *previous design iterations the user explicitly rejected*. Don't propose ideas from them.

---

## What's decided (do not relitigate)

These were debated and locked in during the design phase. If a task seems to want you to change them, **stop and ask the user first**.

- **Three-layer architecture** — Layer 1 (functions), Layer 2 (registry), Layer 3 (client-server interface).
- **The Core is stateless w.r.t. sessions.** Sessions are *not* managed by Core. Clients own their `context` dict and pass it on each request.
- **Every function has signature `(Tokens, Context) -> (Tokens, Context)`** where Tokens is `bytes`.
- **Closure ≠ Context.** Closure is owned by the function (set at construction, not modified). Context is owned by the caller (mutable across calls).
- **CompositeFunction holds a registry reference** via dependency injection. This was deliberate; don't switch to a global registry.
- **CalculateFunction is not JSON-serializable** (Python callables can't safely round-trip). Persistence skips them with a warning. Don't try to pickle them or implement a hack.
- **Singleton LLM client** at module level (`ai_core.llm_client.llm_client`). Tests patch this attribute via `monkeypatch.setattr`.
- **uv + FastAPI + pydantic v2 + pytest** — tech stack chosen by the user, no swaps.

---

## What's open (improvement targets)

The user explicitly flagged these as "later" or "TBD". You're welcome to design or implement these — but **propose first, then implement**, don't just barrel in.

- **Streaming LLM responses** — current shape returns full text. A streaming variant would change function signatures.
- **Async execution** — everything is synchronous. Long term, async makes more sense for a server. Migration plan TBD.
- **Function versioning / namespaces** — currently just flat IDs. The user mentioned `llm/claude_v1` style. Not yet designed.
- **Core portability / serialization** — `save_core` / `load_core` exist for v1 (JSON), but the user wants Core to be "fully portable". Limits: CalculateFunction can't be saved; system paths are absolute. Open question how to handle these.
- **API authentication** — none. Add when needed.
- **Resource scheduling** — `metadata.resource_profile` is descriptive only, not enforced. The user said "secondary problem" but it's a future feature.
- **Multi-process / OS-level isolation for sessions** — discussed but deferred. Sessions are client-side only for now.

If you implement any of these, update `ARCHITECTURE.md` and remove the entry here.

---

## Conventions for *all* code in this repo

These are the user's expressed preferences. Follow them.

- **Type hints everywhere.** Don't widen types to `Any` to silence checkers.
- **No comments restating what code does.** Only comment *why* if non-obvious.
- **No multi-paragraph docstrings.** One-line is fine; the existing skeletons set the bar.
- **No emojis in code or docstrings.** (User-facing prose can have them when the user asks.)
- **No `# noqa` to silence lint.** Fix the underlying issue.
- **Don't create README/markdown files unless asked.** The repo has enough docs.
- **Don't reformat existing files** as a side effect of an edit.
- **Don't add features that weren't asked for.** No "while I'm here" cleanups.

---

## Workflow expectations

1. **Use TaskCreate / TaskUpdate** for any work that takes more than a couple of edits. The user likes seeing progress.
2. **Make small commits.** One logical change per commit. Don't bundle unrelated work.
3. **Don't push to `main`.** The user keeps `main` clean. Work on `try` (or another branch).
4. **Don't run `git --force`, `reset --hard`, or destructive operations** without asking.
5. **Tests are sacred.** Don't modify a test to make it pass. If a test seems wrong after careful reading, stop and ask.
6. **`raise NotImplementedError` means "the user expects this to be filled in".** Don't delete it as "dead code".

---

## Traps and gotchas

These have bitten previous agents. Don't repeat them.

### 1. The LLM client patch needs a fresh import

`tests/conftest.py:mock_llm_client` patches `ai_core.llm_client.llm_client`. If `LLMFunction.__call__` does `from ai_core.llm_client import llm_client` at **module top**, the test will use the unpatched original.

**Solution**: import inside `__call__`. The skeleton's docstring already says this; don't "fix" it by hoisting the import.

### 2. CompositeFunction's registry is not serializable

`CompositeFunction.to_dict()` must drop the `registry` reference. `from_dict()` requires the registry to be passed back in. The persistence layer does this in two passes: load non-composites first, then composites with the freshly-built registry.

Don't try to serialize the registry. It's a big graph.

### 3. The `src/` layout means imports use the package name, not relative paths

The repo uses `src/ai_core/` layout. Imports are always `from ai_core...`, never `from .` or `from src...`. `pyproject.toml`'s `pythonpath = ["src"]` makes this work for tests.

### 4. `tokens` are bytes, not str

`tokens.decode()` to get text, `text.encode()` to go back. Functions that return `str` should encode before returning. Tests will catch this; don't bypass them.

### 5. `bash echo -n` matters

The example shell script uses `echo -n` to avoid trailing newlines. If you write a new shell script for tests, do the same — otherwise byte comparisons fail.

### 6. `httpx.ASGITransport`, not `WSGITransport`

FastAPI is ASGI. `tests/test_service/test_client.py` already does this correctly; don't "simplify" by switching.

---

## Validation — how to know you're done

After any code change:

```bash
# 1. Syntax (fast)
python3 -m py_compile $(find src tests -name "*.py")

# 2. Lint
uv run ruff check src/ai_core/

# 3. Tests
uv run pytest -v

# 4. Examples that don't need API keys
uv run python examples/01_basic_usage.py
uv run python examples/02_composite.py
```

All four must pass. If `pytest` fails, **don't** mark the task complete. Either fix it or ask.

For changes that affect the HTTP API:

```bash
# Run the server
uv run python examples/03_run_server.py

# In another shell
curl -X POST http://localhost:8000/execute \
    -H 'Content-Type: application/json' \
    -d '{"func_id":"upper_v1","tokens":"aGVsbG8=","context":{}}'
```

Should return base64-encoded `HELLO`.

---

## When in doubt

- **Stuck on a contradiction** between two docs → trust `ARCHITECTURE.md` over `SYSTEM_DESIGN.md` (the latter is the philosophy; the former is the build spec).
- **A test seems wrong** → stop. Don't modify the test. Ask the user.
- **You think the design is wrong** → say so. The user has shown they're willing to redesign (this repo's `old/` proves it). But propose, don't unilaterally refactor.
- **A task is ambiguous** → ask one question. Don't guess.

The user pays for tokens. Don't waste them by going in circles or producing something they'll have to throw away.

---

## Quick map

| File | Purpose |
|------|---------|
| `CLAUDE.md` | Vision + user prefs |
| `SYSTEM_DESIGN.md` | Three-layer philosophy |
| `SYSTEM_DESIGN_EXTRA.md` | Design conclusion |
| `ARCHITECTURE.md` | Concrete spec (the build target) |
| `IMPLEMENTATION_PLAN.md` | Step-by-step skeleton-filling tasks |
| `for_agent.md` | This file |
| `pyproject.toml` | uv project config |
| `src/ai_core/` | All source code |
| `tests/` | Pytest suite (concrete assertions) |
| `examples/` | Runnable smoke tests |
| `scripts/shell/` | Example shell scripts |
| `old/` | Rejected design iterations — read for context only |

---

**Last updated**: 2026-04-25 by the architecture pass on branch `try`.
Update this file when you finish work that changes its content.
