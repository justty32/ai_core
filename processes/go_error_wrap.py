#!/usr/bin/env python3
"""
Go error handling improver — suggest idiomatic error wrapping and handling.
Input: Go code with error handling
Output: improved version with fmt.Errorf %w, sentinel errors, error types
"""
import sys
import json
import os


METADATA = {
    "name": "go_error_wrap",
    "description": "Improve Go error handling: wrap with context, use sentinel errors, errors.Is/As",
    "version": "1.0",
    "tags": ["go", "golang", "error-handling", "errors"],
    "input": "go source code",
    "output": "improved go source code with better error handling",
}

SYSTEM_PROMPT = """You are an expert Go engineer specializing in error handling.
Improve the error handling in the given Go code:
- Wrap errors with context using fmt.Errorf("doing X: %w", err)
- Define sentinel errors (var ErrXxx = errors.New(...)) where appropriate
- Use errors.Is() and errors.As() for checking wrapped errors
- Return errors up the call stack; don't swallow them silently
- Add error types for structured error data when needed
Output the improved code with a brief comment on each change."""


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
