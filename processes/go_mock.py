#!/usr/bin/env python3
"""
Go mock generator — generate a mock struct implementing a given interface.
Input: Go interface definition
Output: mock struct with method stubs (compatible with testify/mock)
"""
import sys
import json
import os


METADATA = {
    "name": "go_mock",
    "description": "Generate a testify/mock compatible mock struct for a Go interface",
    "version": "1.0",
    "tags": ["go", "golang", "mock", "test", "testify"],
    "input": "go interface definition",
    "output": "go mock struct implementation",
}

SYSTEM_PROMPT = """You are an expert Go engineer.
Generate a testify/mock compatible mock for the given Go interface.
Requirements:
- Embed mock.Mock
- Each method calls Called() and returns the mock's return values
- Use correct types from the interface
- Add a constructor New<MockName>() for convenience
- Output only the mock code in a _mock_test.go style file"""


def run(interface_code: str) -> str:
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    import litellm
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"```go\n{interface_code}\n```"},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        code = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(code))
