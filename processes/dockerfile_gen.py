#!/usr/bin/env python3
"""
Dockerfile generator — generate optimized Dockerfile for a Go service.
Input: description of the service (or paste go.mod)
Output: multi-stage Dockerfile
"""
import sys
import json
import os

METADATA = {
    "name": "dockerfile_gen",
    "description": "Generate an optimized multi-stage Dockerfile for a Go service",
    "version": "1.0",
    "tags": ["docker", "go", "golang", "devops", "codegen"],
    "input": "service description or go.mod content",
    "output": "multi-stage dockerfile",
}

SYSTEM_PROMPT = """You are an expert Go/Docker engineer.
Generate an optimized multi-stage Dockerfile for a Go service.
Requirements:
- Stage 1 (builder): golang:1.23-alpine, copy go.mod/go.sum first for cache, then build
- Stage 2 (final): gcr.io/distroless/static-debian12 or scratch for minimal size
- Set CGO_ENABLED=0 GOOS=linux for static binary
- Run as non-root user
- EXPOSE the service port
- Add HEALTHCHECK if it's an HTTP service
- Use .dockerignore friendly COPY patterns
Output only the Dockerfile."""


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
