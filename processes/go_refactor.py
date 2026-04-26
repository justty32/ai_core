#!/usr/bin/env python3
"""
Go refactor suggester — suggest and apply targeted refactors to Go code.
Input: Go code + optional refactor goal (e.g. "extract interface", "reduce duplication")
Output: refactored code with explanation
"""
import sys
import json
import os

METADATA = {
    "name": "go_refactor",
    "description": "Suggest and apply refactors to Go code: extract interface, reduce duplication, improve naming",
    "version": "1.0",
    "tags": ["go", "golang", "refactor", "code-quality"],
    "input": "go source code (optionally with refactor goal)",
    "output": "refactored go code with explanation",
}

SYSTEM_PROMPT = """You are an expert Go engineer doing a targeted refactor.
Refactor the given Go code to improve structure and readability.
Focus on:
- Extracting interfaces where useful
- Eliminating duplication (table-driven patterns, helper functions)
- Improving naming (exported names, unexported helpers)
- Simplifying complex conditionals
- Reducing function length (single responsibility)

Output:
1. The refactored code
2. A brief bulleted list of what changed and why"""


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
