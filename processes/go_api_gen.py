#!/usr/bin/env python3
"""
Go HTTP API handler generator — generate net/http or chi/gin handler from description.
Input: description of the endpoint (method, path, request/response shape)
Output: Go handler function with proper error handling
"""
import sys
import json
import os

METADATA = {
    "name": "go_api_gen",
    "description": "Generate a Go HTTP handler (net/http style) from an endpoint description",
    "version": "1.0",
    "tags": ["go", "golang", "http", "api", "handler", "codegen"],
    "input": "endpoint description (method, path, request/response)",
    "output": "go http handler code",
}

SYSTEM_PROMPT = """You are an expert Go engineer.
Generate a Go HTTP handler for the described endpoint.
Requirements:
- Use standard net/http signature: func(w http.ResponseWriter, r *http.Request)
- Decode JSON request body with error handling
- Return proper HTTP status codes
- Encode JSON response
- Use a service/repository interface for business logic (don't inline DB calls)
- Handle context cancellation
Output the handler + any request/response structs needed."""


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
