#!/usr/bin/env python3
"""
Go concurrency reviewer — review goroutines, channels, and sync primitives.
Input: Go code with concurrency patterns
"""
import sys
import json
import os

METADATA = {
    "name": "go_concurrency_review",
    "description": "Review Go concurrency: goroutine leaks, race conditions, channel patterns, sync usage",
    "version": "1.0",
    "tags": ["go", "golang", "concurrency", "goroutine", "channel", "review"],
    "input": "go source code with concurrency",
    "output": "concurrency review comments",
}

SYSTEM_PROMPT = """You are an expert Go engineer specializing in concurrency.
Review the code for:
- Goroutine leaks (goroutines that can never exit)
- Race conditions (shared state without synchronization)
- Channel misuse (unbuffered vs buffered, close patterns)
- Mutex/RWMutex correctness (defer unlock, lock ordering deadlocks)
- Context cancellation propagation
- sync.WaitGroup correctness (Add before goroutine, not inside)
- Use of sync/atomic where appropriate

Be specific: name the line or variable causing the issue."""


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
