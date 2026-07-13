# doc_17_arch_changelog (Vernacular Version)

- **Source**: `docs/architectures/08_changelog.md`
- **Status**: Legacy Arch Doc
- **TL;DR**: A history of how this specific "legacy" branch of the architecture evolved, what we added, and what we eventually threw away.

---

## The Highlight Reel

### April 2026: The Big Bang
- Defined the core components (Server, Call, Hub, SFC).
- Created the **Singleton Resource Manager Pattern** to handle LLM rate limits and queues.
- Introduced the `--metadata` and `--json-errors` protocols so scripts could talk to each other without breaking.
- Added **Agent integration** (AGENTS.md, auto-docs).

### May 2026: Trimming the Fat
- **Removed the Ledger**: We tried to track every single LLM call (Calling Chain Observability), but realized it was way too complex for the first version. `ai-core-trace` and `ledger_path` were deleted.
- **Unified the protocol**: Decided that "Calling Packs" are just regular functions in the `funcs/` folder—no need for a new architectural concept.
- **Improved Paths**: Standardized default paths to use `user_data_dir` and allowed environment variable overrides (`AI_CORE_FUNCS_DIR`).

---

## Notable Decisions
- **Dry-run by default**: `ai-core-author` must verify code against examples before registering it.
- **Python for Core, anything for functions**: The system itself is Python, but your functions can be Bash, Python, or even a compiled binary—as long as they speak the `--metadata` protocol.
- **HTTP API cleanup**: Simplified the endpoints (e.g., `GET /entries/<name>` instead of `/entries/<name>/metadata`).
