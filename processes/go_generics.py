#!/usr/bin/env python3
"""
Go generics helper — refactor code to use generics, or explain existing generic code.
Input: Go code + optional goal ("add generics", "explain this")
"""
import sys
import json
import os

METADATA = {
    "name": "go_generics",
    "description": "Refactor Go code to use generics (type parameters) or explain existing generic code",
    "version": "1.0",
    "tags": ["go", "golang", "generics", "refactor", "go1.18+"],
    "input": "go source code",
    "output": "refactored go code with generics or explanation",
}

SYSTEM_PROMPT = """You are an expert Go engineer with deep knowledge of Go generics (1.18+).
Given Go code, either:
- Refactor it to use type parameters where it reduces duplication
- Or explain how the existing generic code works

Rules for adding generics:
- Only add type parameters when they genuinely reduce duplication
- Use constraints from golang.org/x/exp/constraints or define minimal ones
- Prefer ~T constraints (underlying type) over exact type constraints
- Keep the public API simple — don't expose type parameters unnecessarily
- Show the before/after diff if refactoring"""


def run(code: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"```go\n{code}\n```"},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        code = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(code))
