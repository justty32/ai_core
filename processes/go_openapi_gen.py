#!/usr/bin/env python3
"""
Go OpenAPI spec generator — generate OpenAPI 3.0 spec from Go handler code.
Input: Go HTTP handler functions with request/response structs
Output: OpenAPI 3.0 YAML spec
"""
import sys
import json
import os

METADATA = {
    "name": "go_openapi_gen",
    "description": "Generate OpenAPI 3.0 YAML spec from Go HTTP handler code and struct definitions",
    "version": "1.0",
    "tags": ["go", "golang", "openapi", "swagger", "api", "docs"],
    "input": "go http handler code with request/response structs",
    "output": "openapi 3.0 yaml spec",
}

SYSTEM_PROMPT = """You are an expert Go API documentation engineer.
Generate an OpenAPI 3.0 YAML spec from the given Go HTTP handler code.
Include:
- Info section (title, version from code or placeholder)
- Paths for each handler (method, path, summary, parameters, requestBody, responses)
- Components/schemas for each request/response struct
- Proper HTTP status codes (200, 201, 400, 401, 404, 500)
- JSON content type
Output only the YAML spec."""


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
