# doc_16_arch_agents (Vernacular Version)

- **Source**: `docs/architectures/07_agents.md`
- **Status**: Legacy Arch Doc
- **TL;DR**: How we make sure AI agents (like Claude Code, Cursor, Aider) can actually use this system without getting lost.

---

## 15. The "Agents are First-Class Citizens" Strategy

We don't just build this for humans; we build it for AI agents. The goal is to have a collection of functions so good that an agent can just pick them up and do the work for you.

### Documentation Layers
We split docs into two types:
1. **Hand-written**: The "why" and "how-to" (README, AGENTS.md, etc.).
2. **Auto-generated**: The "what's available right now" (found in the `auto/` folder). 
   - Never hand-edit the `auto/` folder. It's generated from your functions' metadata.

---

## 15.2 AGENTS.md: The Welcome Mat for AI
Every project should have an `AGENTS.md` at the root. When an agent enters the repo, it should read this first. It tells them:
- How to call the LLM (`ai-core-call`).
- How to see what tools are available (`ai-core-hub --build-list`).
- How to create new tools (`ai-core-author`).

---

## 15.3 Exporting to Agent Formats
The Hub can export your function list into formats that specific agents love:
- **OpenAI/Anthropic Tools**: For standard API-based agents.
- **MCP (Model Context Protocol)**: For Claude Desktop and other MCP-savvy tools.
- **Claude Skills**: Specifically for Claude Code's `SKILL.md` format.

---

## 15.4 Keep it Fresh
The documentation should update automatically:
- When you register a new function via `ai-core-author`, it triggers a doc rebuild.
- We can even put a check in `pre-commit` to make sure the docs match the actual code.

---

## 15.5 The Evolution Path
1. **Agent-assisted**: Human asks, agent writes a script. (Where we are now).
2. **Function-rich**: Most tasks already have a function. Agent just orchestrates them.
3. **Self-sufficient**: The system recognizes patterns and creates its own "chains" of functions.

---

## 15.6 Quick Integration Tips
- **Claude Code**: Uses `CLAUDE.md` and the `auto/skills/` folder.
- **Cursor/Cline**: Point them to `AGENTS.md` via `.cursorrules`.
- **Aider**: Add `AGENTS.md` to the chat context.
