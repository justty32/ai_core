# doc_20 — Taming the LLM (Vernacular Version)

- **Source**: `try_implement/docs/llm_taming_framework.md`
- **Status**: Core Philosophy
- **TL;DR**: Most code is predictable. LLMs are not. This document explains how we wrap the "chaos" of LLMs into reliable, predictable functions that you can actually trust in a production pipeline.

---

## 0. The Core Idea
Think of `ai_core` as a machine that takes a "random" function (the LLM) and processes it through several layers until it becomes a "reliable" function.

---

## 1. The Problem: LLMs are Flaky
In our system, every function is `f(input) -> output`. Most functions give the same output for the same input. LLMs don't. 
- **The Bad**: It's hard to debug, hard to test, and inconsistent.
- **The Good**: It's creative and flexible.
**The Goal**: We don't want to kill the creativity; we just want to "tame" it so it behaves when we need it to.

---

## 2. The Five Layers of Reliability

### L0: The Contract (Narrow the playing field)
The more freedom you give an LLM, the more ways it can fail. 
- **Fix**: Force it to output JSON or specific keywords (Yes/No). 
- **Pro-tip**: Use `--metadata` to define strict input/output types. Don't ask for an essay if you only need a status code.

### L1: Determinism (Freeze the chaos)
- **Memoization**: If we've seen this exact prompt before, just return the cached answer. This makes the LLM feel like a "real" function and saves money.
- **Temperature 0**: Tell the model to stop being "creative" and just give the most likely answer.

### L2: Validation (Trust but verify)
- **Retry**: If the output is garbage, throw it away and try again (up to N times).
- **Guard/Repair**: If the output is *almost* right but has a small error (like a missing bracket), send it back to the LLM with a "Hey, fix this" message.

### L3: Aggregation (Wisdom of the crowd)
- **Voting**: Ask the same question 3 times and take the majority answer.
- **Best of N**: Generate 5 versions and use a "scorer" (another LLM or a script) to pick the winner.

### L4: Orchestration (Set boundaries)
Instead of asking the LLM to do everything, give it a toolbox (the **Hub**).
- The LLM's job isn't to "know" everything; it's to "decide" which deterministic tool to use. The script does the heavy lifting; the LLM just steers.

---

## 3. The "Reliable QA" Example
How to build a coding assistant that doesn't hallucinate broken code:
1. **L0**: Bind the prompt to require a ```python``` block.
2. **L2 (Retry)**: If there's no code block, try again.
3. **L2 (Guard)**: Run a syntax check on the code. If it fails, ask the LLM to fix it.
4. **L1 (Cache)**: If the same question comes in tomorrow, return the fixed code instantly.

**Result**: A "flaky" model becomes a "rock-solid" coding tool.
