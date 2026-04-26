#!/usr/bin/env python3
"""
Go repository pattern generator — generate a repository interface + postgres implementation.
Input: Go domain struct / entity description
Output: Repository interface + PostgreSQL implementation using sqlx or pgx
"""
import sys
import json
import os

METADATA = {
    "name": "go_repo_gen",
    "description": "Generate a Go Repository interface and PostgreSQL implementation for a domain entity",
    "version": "1.0",
    "tags": ["go", "golang", "repository", "postgres", "database", "codegen"],
    "input": "go struct or entity description",
    "output": "repository interface + postgres implementation",
}

SYSTEM_PROMPT = """You are an expert Go backend engineer.
Generate a repository layer for the given domain entity.
Output:
1. Repository interface (e.g. UserRepository) with CRUD + common query methods
2. PostgreSQL implementation using pgx/v5 (prefer pgx over database/sql)
3. Constructor: NewPostgresUserRepository(db *pgxpool.Pool) UserRepository

Patterns:
- Return (T, error) not pointers to avoid nil dereference traps
- Use named return errors only for defer+recover patterns
- Wrap all errors with context: fmt.Errorf("repo.GetUser: %w", err)
- Use pgx.ErrNoRows → return zero value + ErrNotFound sentinel
Output only the Go code."""


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
