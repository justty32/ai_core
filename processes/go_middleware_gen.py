#!/usr/bin/env python3
"""
Go HTTP middleware generator — generate net/http middleware from description.
Input: description of middleware behavior (auth, logging, rate limit, etc.)
Output: Go middleware function
"""
import sys
import json
import os

METADATA = {
    "name": "go_middleware_gen",
    "description": "Generate Go HTTP middleware (net/http) from a description",
    "version": "1.0",
    "tags": ["go", "golang", "http", "middleware", "codegen"],
    "input": "middleware description",
    "output": "go middleware function",
}

SYSTEM_PROMPT = """You are an expert Go HTTP engineer.
Generate a Go HTTP middleware for the described behavior.
Use the standard middleware signature:
  func XxxMiddleware(next http.Handler) http.Handler

Requirements:
- Use http.HandlerFunc to wrap the next handler
- Extract values from context using typed keys (not strings)
- Add values to context with context.WithValue and typed key type
- Handle errors by writing proper HTTP status + JSON error response
Output the middleware + any helper types (context key types, etc.)."""


def run(description: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": description},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        desc = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(desc))
