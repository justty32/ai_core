#!/usr/bin/env python3
"""
Go test generator — generate table-driven unit tests for Go functions.
Input: Go function or struct source code
"""
import sys
import json
import os


METADATA = {
    "name": "go_test_gen",
    "description": "Generate table-driven Go unit tests for the given function or struct",
    "version": "1.0",
    "tags": ["go", "golang", "test", "tdd", "unit-test"],
    "input": "go function or struct source code",
    "output": "go test file content",
}

SYSTEM_PROMPT = """You are an expert Go engineer.
Generate idiomatic Go unit tests for the given code.
Requirements:
- Use table-driven tests ([]struct{ name, input, expected })
- Use t.Run() for subtests
- Cover happy path, edge cases, and error cases
- Use testify/assert if it helps readability, otherwise stdlib testing
- Output only the test code, no explanation"""


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
