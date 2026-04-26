#!/usr/bin/env python3
"""
Go interface extractor — infer a minimal interface from struct method usage.
Input: Go struct with methods, or usage code
Output: minimal interface definition
"""
import sys
import json
import os


METADATA = {
    "name": "go_interface",
    "description": "Infer a minimal Go interface from a struct's methods or usage patterns",
    "version": "1.0",
    "tags": ["go", "golang", "interface", "design"],
    "input": "go struct or usage code",
    "output": "go interface definition",
}

SYSTEM_PROMPT = """You are an expert Go engineer.
Given Go code (a struct with methods, or code that uses a type),
extract or infer the minimal interface that the code depends on.
Follow Go interface design principles:
- Small, focused interfaces (prefer 1-3 methods)
- Name interfaces by behavior (Reader, Storer, not IFoo)
- Accept interfaces, return structs
Output only the interface definition(s) with a short comment explaining the intent."""


def run(code: str) -> str:
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    import litellm
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
