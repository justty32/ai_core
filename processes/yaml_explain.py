#!/usr/bin/env python3
"""
YAML explainer — explain a YAML config file (k8s, docker-compose, CI, etc.).
Input: YAML content
Output: plain language explanation of each section
"""
import sys
import json
import os

METADATA = {
    "name": "yaml_explain",
    "description": "Explain a YAML config file (k8s, docker-compose, GitHub Actions, etc.) in plain language",
    "version": "1.0",
    "tags": ["yaml", "k8s", "docker", "ci", "explain", "devops"],
    "input": "yaml file content",
    "output": "plain language explanation",
}

SYSTEM_PROMPT = """Explain the given YAML configuration file clearly.
- Identify what system/tool it configures (k8s, docker-compose, GitHub Actions, etc.)
- Explain each top-level section and its purpose
- Highlight important or non-obvious settings
- Note any security concerns or common gotchas
Be concise. Use a structured format matching the YAML hierarchy."""


def run(yaml_text: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": yaml_text},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
