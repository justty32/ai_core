#!/usr/bin/env python3
"""
Error diagnostician — diagnose error messages and suggest fixes.
Input: error message or stack trace (any language)
Output: diagnosis and suggested fix
"""
import sys
import json
import os

METADATA = {
    "name": "error_diagnose",
    "description": "Diagnose error messages and stack traces, suggest root cause and fix",
    "version": "1.0",
    "tags": ["debug", "error", "diagnose", "utility"],
    "input": "error message or stack trace",
    "output": "diagnosis and fix suggestions",
}

SYSTEM_PROMPT = """You are an expert debugger.
Diagnose the given error message or stack trace:
1. Root cause: what actually went wrong (1-2 sentences)
2. Most likely fix: the concrete change to make
3. Alternative causes: if there are other possibilities
4. Prevention: how to avoid this class of error

Be direct. Skip generic advice. Focus on the specific error."""


def run(error: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": error},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
