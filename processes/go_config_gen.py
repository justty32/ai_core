#!/usr/bin/env python3
"""
Go config struct generator — generate config struct loaded from env vars.
Input: list of config fields needed (or description)
Output: Go config struct + loader using os.Getenv / godotenv
"""
import sys
import json
import os

METADATA = {
    "name": "go_config_gen",
    "description": "Generate a Go config struct loaded from environment variables",
    "version": "1.0",
    "tags": ["go", "golang", "config", "env", "12factor", "codegen"],
    "input": "list of config fields or service description",
    "output": "go config struct with env loader",
}

SYSTEM_PROMPT = """You are an expert Go engineer following 12-factor app principles.
Generate a Go config struct loaded from environment variables.
Requirements:
- Config struct with all fields
- LoadConfig() function that reads from os.Getenv
- Validate required fields (return error if missing)
- Provide defaults for optional fields
- Use typed fields (not all strings): int for ports, time.Duration for timeouts, bool for flags
- Add a String() method that masks sensitive values (passwords, keys)
Output the config.go file content."""


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
