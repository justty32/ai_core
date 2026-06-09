# doc_21 — Snapping Functions Together (Vernacular Version)

- **Source**: `try_implement/docs/multi_function_interaction.md`
- **Status**: Prototype / Proposal
- **TL;DR**: Our "Nine Axes" describe what a single function does. "Composition" describes how you snap those functions together like Legos to build something bigger.

---

## 1. Beyond the "Single Function"
If you only have single functions, you just have a bunch of scripts. The real power comes when you combine them. We need a standard way to say "Run A, then B, and if it fails, run C."

---

## 2. The Basic Snap-on Tools (Combinators)
In `lib/compose.py`, we've built "combinators" that take your functions and wrap them in new logic.

| Style | Tool | What it does |
|---|---|---|
| **Pipeline** | `pipe(A, B, C)` | Runs A, feeds output to B, then B to C. |
| **Parallel** | `fanout(A, B)` | Runs A and B at the same time with the same input. |
| **Switch** | `route(selector, table)` | Picks which function to run based on the input. |
| **Divide & Conquer** | `decompose` | Splits a big task into small ones, runs them, and joins them back. |
| **Looping** | `retry` / `vote` | Runs the same function multiple times to get a better result. |

---

## 3. The "Meta" Problem: What is the new function's nature?
If you pipe an "idempotent" function (safe to retry) into a "non-idempotent" one (risky to retry), is the whole pipeline safe to retry?
**The Rule**: A chain is only as strong as its weakest link.
- **Guarantee**: If one part isn't idempotent, the whole chain isn't.
- **State**: If any function in the chain touches the disk, the whole chain is "stateful."

---

## 4. Interaction vs. Composition
- **Composition**: A simple one-way flow of data (A -> B -> C).
- **Interaction**: A back-and-forth conversation (A <-> B).

### The "Blackboard" Model
For complex tasks (like an actor-critic loop where one AI writes and another critiques), we use a "Blackboard":
1. Everyone sits around a "shared state" (the blackboard).
2. They take turns reading and writing to it.
3. A "Safety Valve" (max rounds) makes sure they don't talk forever in a loop.

---

## 5. Open Questions
- Should the Hub automatically calculate the "nature" (metadata) of a pipeline? (We have a prototype for this).
- Should we allow the AI to "compose" its own pipelines on the fly? (Powerful, but potentially dangerous).
