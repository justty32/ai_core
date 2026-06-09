# doc_15_arch_project_milestones (Vernacular Version)

- **Source**: `docs/architectures/06_project.md`
- **Status**: Legacy Arch Doc (Reality has shifted quite a bit)
- **TL;DR**: This document covers the original directory structure, the CLI entrypoints, and the "M0 to M6" game plan for building the system.

---

## 10. The Original Vision for the Directory Tree
The old plan had a very modular structure with separate folders for `entry_manager`, `hub`, `sfc`, and `author`. 
**Reality Check**: Right now, we're keeping it lean. Most of the core logic lives in `src/ai_core/_core.py`. Don't get confused by the old multi-folder layout mentioned in these docs.

### Protocol Helpers
We wanted a `protocol/` module to handle all the boring stuff:
- `metadata.py`: Helpers for reading/writing those `--metadata` JSONs.
- `env.py`: Standardized environment variable names (`AI_CORE_TOKEN`, etc.).
- **Rule of thumb**: If you're building a function or a manager, use these helpers instead of reinventing the wheel.

---

## 11. CLI Entrypoints
The plan was to have several CLI tools installed via `pyproject.toml`:
- `ai-core-server`: The main engine.
- `ai-core-call`: A wrapper to talk to the server easily.
- `ai-core-hub`: To scan and find available functions.
- `ai-core-sfc`: To manage groups of tiny functions.
- `ai-core-author`: To generate new functions using AI.

---

## 13. The Development Roadmap (The "M" Milestones)
*Note: This is the old roadmap. The new one in `roadmap.md` is what we actually follow.*

### M0 - The Basics
Setting up the repo, the protocol, and making sure a simple `echo.sh` works with the `--metadata` flag.

### M1 & M2 - The Server & Real Backends
Getting the FastAPI server running, connecting to LLMs (Ollama, Gemini, etc.), and handling queues and rate limits.

### M3 - The Function Hub
Building the scanner that finds every script in your `funcs/` folder and turns them into a list the AI can understand (MCP, OpenAI tools, etc.).

### M4 & M5 - Scaling Up
Adding SFC (Small Function Center) to handle tiny functions and `ai-core-author` to let the system "grow" by writing its own functions.

---

## 14. A Note on Windows
Originally, we thought about full Windows support (msvcrt, ACLs, etc.). 
**Current Stance**: We are focusing on **POSIX (Linux/macOS)**. Windows isn't the priority right now, so keep your scripts shell-friendly and avoid Windows-specific hacks unless you really have to.
