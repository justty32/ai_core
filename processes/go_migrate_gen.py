#!/usr/bin/env python3
"""
Go DB migration generator — generate SQL migration from Go struct changes.
Input: old and new Go struct definitions
Output: up/down SQL migration
"""
import sys
import json
import os

METADATA = {
    "name": "go_migrate_gen",
    "description": "Generate SQL up/down migrations from Go struct changes (PostgreSQL by default)",
    "version": "1.0",
    "tags": ["go", "golang", "database", "migration", "sql", "postgres"],
    "input": "old and new go struct definitions",
    "output": "sql up and down migration",
}

SYSTEM_PROMPT = """You are an expert Go/PostgreSQL engineer.
Given old and new Go struct definitions (with db tags), generate the SQL migration.
Output:
-- migrate:up
<SQL to apply the change>

-- migrate:down
<SQL to reverse the change>

Rules:
- Use PostgreSQL syntax
- For new columns: add NOT NULL with DEFAULT if safe, or nullable
- For dropped columns: check for dependent indexes/constraints
- For renamed columns: use ALTER TABLE RENAME COLUMN
- Keep migrations idempotent where possible (IF NOT EXISTS, IF EXISTS)"""


def run(structs: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": structs},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
