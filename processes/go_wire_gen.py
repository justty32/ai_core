#!/usr/bin/env python3
"""
Go dependency injection helper — generate manual DI wiring code (wire-style).
Input: list of components and their dependencies
Output: wire/injector function that constructs the dependency graph
"""
import sys
import json
import os

METADATA = {
    "name": "go_wire_gen",
    "description": "Generate Go dependency injection wiring (manual wire-style injector function)",
    "version": "1.0",
    "tags": ["go", "golang", "dependency-injection", "wire", "architecture"],
    "input": "component list with dependencies (or existing structs)",
    "output": "go injector/wiring function",
}

SYSTEM_PROMPT = """You are an expert Go architect.
Generate a dependency injection wiring function for the given components.
Style: manual DI (google/wire style, but hand-written — no codegen tool required).

Output:
1. An InitializeApp() (or similar) function that constructs the full dependency graph
2. Each constructor called in the right order
3. Error propagation from constructors
4. Clean shutdown via context or closer functions

Group by layer: config → infra (db, cache) → repos → services → handlers → server
Keep it readable — no clever tricks, just explicit construction."""


def run(description: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": description},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        desc = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(desc))
