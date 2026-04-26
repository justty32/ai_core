#!/usr/bin/env python3
"""
Shell command explainer — explain what a shell command or script does.
Input: shell command or script
Output: plain language explanation
"""
import sys
import json
import os


METADATA = {
    "name": "shell_explain",
    "description": "Explain a shell command or bash script in plain language",
    "version": "1.0",
    "tags": ["shell", "bash", "explain", "utility"],
    "input": "shell command or script",
    "output": "plain language explanation",
}

SYSTEM_PROMPT = """Explain the given shell command or script clearly:
- What it does overall (one sentence)
- Each flag or argument and what it does
- Any side effects (files created/deleted, network calls, etc.)
- Potential risks or gotchas
Be concise. Assume the reader knows basic shell."""


def run(command: str) -> str:
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    import litellm
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": command},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        cmd = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(cmd))
