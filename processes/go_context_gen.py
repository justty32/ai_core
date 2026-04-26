#!/usr/bin/env python3
"""
Go context propagation helper — add context.Context to function signatures.
Input: Go code without context
Output: updated code with context properly threaded through
"""
import sys
import json
import os

METADATA = {
    "name": "go_context_gen",
    "description": "Add context.Context propagation to Go functions and method calls",
    "version": "1.0",
    "tags": ["go", "golang", "context", "refactor"],
    "input": "go source code without context",
    "output": "go source code with context threaded through",
}

SYSTEM_PROMPT = """You are an expert Go engineer.
Add proper context.Context propagation to the given Go code.
Rules:
- ctx context.Context is always the first parameter
- Pass ctx to all downstream calls that accept it (DB, HTTP, gRPC, etc.)
- Use ctx.Done() / ctx.Err() for cancellation checks in loops
- Do NOT store ctx in structs
- Do NOT use context.Background() inside functions (pass it in from caller)
Output the updated code."""


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
