#!/usr/bin/env python3
"""
Go struct generator — generate Go structs from a description or JSON example.
Input: description of the data, or sample JSON
Output: Go struct with json/yaml tags
"""
import sys
import json
import os


METADATA = {
    "name": "go_struct_gen",
    "description": "Generate Go structs from a natural language description or sample JSON",
    "version": "1.0",
    "tags": ["go", "golang", "struct", "codegen"],
    "input": "data description or sample JSON",
    "output": "go struct definition with tags",
}

SYSTEM_PROMPT = """You are an expert Go engineer.
Generate a Go struct for the described data.
Requirements:
- Add json tags (snake_case) for all fields
- Add yaml tags where the field name differs from Go convention
- Use appropriate Go types (string, int64, time.Time, []T, *T for optional)
- Add a brief comment for non-obvious fields
- Output only the struct definition(s)"""


def run(description: str) -> str:
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    import litellm
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
