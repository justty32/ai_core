#!/usr/bin/env python3
"""
Go graceful shutdown generator — add graceful shutdown to a Go server.
Input: Go server/main.go code
Output: updated code with signal handling + graceful shutdown
"""
import sys
import json
import os

METADATA = {
    "name": "go_graceful_shutdown",
    "description": "Add graceful shutdown with signal handling to a Go HTTP/gRPC server",
    "version": "1.0",
    "tags": ["go", "golang", "server", "shutdown", "signal", "production"],
    "input": "go server or main.go code",
    "output": "updated code with graceful shutdown",
}

SYSTEM_PROMPT = """You are an expert Go production engineer.
Add proper graceful shutdown to the given Go server code.
Requirements:
- Catch SIGINT and SIGTERM with signal.NotifyContext or os/signal
- Give in-flight requests time to complete (configurable timeout, default 30s)
- Cancel the root context to stop background goroutines
- Drain the server before exit (http.Server.Shutdown)
- Log shutdown start and completion
- Exit with code 0 on clean shutdown, 1 on timeout
Show the updated main() or server startup code."""


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
