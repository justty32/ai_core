# doc_22 — Dogfooding: The Idea Track (Vernacular Version)

- **Source**: `.claude/commands/` and `ideas/`
- **Status**: Live and in-use (Dogfooding phase)
- **TL;DR**: We’re using `ai_core` to build `ai_core`. We’ve added slash commands to Claude to handle our daily workflows and a dedicated "Idea Track" to catch and refine inspirations using our own LLM infrastructure.

---

## 1. Why we did this
We wanted to stop doing repetitive manual work. We moved from "talking to the agent" to "using commands" for common tasks. 
More importantly, we moved the "brainstorming" logic away from a high-level agent and down to our own `idea.py` tool. This is our first real test of using our own "cheap model consumer" infrastructure.

---

## 2. The Slash Commands (Your New Best Friends)
| Command | What it does |
|---|---|
| `/resume` | Rebuilds the context so you can get back to work quickly. |
| `/test` | Runs all the tests (pytest + smoke tests) and gives you a report. |
| `/spec` | Moves code from a "prototype" (try_implement) to "official" (core_nature). |
| `/proto` | Creates a quick prototype to see if an idea actually works. |
| `/intake` | For voice/quick notes: Logs your raw text and organizes it. |
| `/critique` | Asks the LLM to find holes in your latest idea. |
| `/expand` | Asks the LLM to take an idea and run with it. |

---

## 3. The Idea Track: From Raw Note to Structured Brainstorm
All ideas live in the `ideas/` folder. It has a strict 4-step flow:
1. **Raw**: Your exact words, no AI filtering. (Safe and immutable).
2. **Cleaned**: AI removes the "umms" and typos but keeps the original meaning.
3. **Notes**: AI turns the cleaned text into a structured markdown note.
4. **Brainstorm**: The AI critiques or expands on the notes to find new angles.

---

## 4. True Dogfooding
We don't use the main agent's "magic" for the Idea Track. Instead, we call `idea.py`, which:
- Packages the prompt using our own `lib/llm_call`.
- Goes through our **LLM Entry Manager** to handle rate limits.
- Hits the real API (OpenAI/Anthropic) using our own backends.

**The result**: We're proving that our system can actually power a real-world tool.

---

## 5. Lessons Learned (The "Gaps")
Building this exposed some bugs:
- **Rate Limit Reset**: We found that one-shot tools were resetting the rate limit every time. **Fixed**: We now use a long-running socket daemon for the Entry Manager so the "meter" stays active across calls.
- **Nine Axes**: We confirmed that every tool should honestly declare `nondeterministic: true` until it's been proven stable.
