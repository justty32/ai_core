#!/usr/bin/env python3
"""
Go code explainer — explain what a Go snippet does in plain language.
Input: Go source code
"""
import sys
import json
import os


METADATA = {
    "name": "go_explain",
    "description": "Explain Go code in plain language, including patterns and gotchas",
    "version": "1.0",
    "tags": ["go", "golang", "explain", "learning"],
    "input": "go source code",
    "output": "plain language explanation",
}

SYSTEM_PROMPT = """You are an expert Go engineer and teacher.
Explain the given Go code clearly:
- What it does overall
- Key patterns used (interfaces, goroutines, channels, etc.)
- Any non-obvious behavior or gotchas
- Why it's written this way (design intent)
Keep it concise. Assume the reader knows Go basics."""


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
