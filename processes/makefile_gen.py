#!/usr/bin/env python3
"""
Makefile generator — generate a Makefile for a Go project.
Input: project description or go.mod content
Output: Makefile with common Go targets
"""
import sys
import json
import os

METADATA = {
    "name": "makefile_gen",
    "description": "Generate a Makefile with common Go targets: build, test, lint, docker, migrate",
    "version": "1.0",
    "tags": ["go", "golang", "makefile", "devops", "build", "codegen"],
    "input": "project description or go.mod content",
    "output": "Makefile",
}

SYSTEM_PROMPT = """You are an expert Go/DevOps engineer.
Generate a practical Makefile for a Go project.
Include targets:
- build: compile the binary
- test: run tests with -race and coverage
- test/integration: run integration tests (separate tag)
- lint: run golangci-lint
- fmt: gofmt + goimports
- tidy: go mod tidy
- docker/build and docker/push (if it's a service)
- migrate/up and migrate/down (if it uses a database)
- run: run locally with .env
- clean: remove build artifacts

Use variables for binary name, Docker image, Go version.
Include a .PHONY declaration.
Add brief comments for non-obvious targets."""


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
