#!/usr/bin/env python3
"""
Go code review — review Go code for correctness, idioms, and best practices.
Input: Go source code (via stdin or arg)
"""
import sys
import json
import os


METADATA = {
    "name": "go_review",
    "description": "Review Go code for correctness, idioms, error handling, and best practices",
    "version": "1.0",
    "tags": ["go", "golang", "review", "code-quality"],
    "input": "go source code",
    "output": "review comments",
}

SYSTEM_PROMPT = """You are an expert Go engineer doing a code review.
Review the given Go code and provide feedback on:
- Correctness and logic errors
- Go idioms and best practices
- Error handling (proper wrapping, sentinel errors)
- Goroutine and concurrency safety
- Interface design
- Naming conventions
- Performance concerns

Be concise. List issues as bullet points. If the code looks good, say so."""


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
