#!/usr/bin/env python3
"""
Go doc generator — add godoc comments to exported Go symbols.
Input: Go source code
Output: same code with godoc comments added
"""
import sys
import json
import os


METADATA = {
    "name": "go_doc",
    "description": "Add or improve godoc comments for exported Go functions, types, and methods",
    "version": "1.0",
    "tags": ["go", "golang", "docs", "godoc"],
    "input": "go source code",
    "output": "go source code with godoc comments",
}

SYSTEM_PROMPT = """You are an expert Go engineer.
Add or improve godoc comments for all exported symbols (functions, types, methods, constants).
Rules:
- Comments must start with the symbol name
- Be concise but complete
- Document parameters and return values for non-obvious functions
- Output only the updated Go code, no explanation"""


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
