#!/usr/bin/env python3
"""
LLM entry process — raw call to an LLM, no system prompt, no history.
Model is selected via LLM_MODEL env var (default: gemini/gemini-2.5-flash).
"""
import sys
import json
import os


METADATA = {
    "name": "llm_call",
    "description": "Send input text to an LLM and return the response",
    "version": "1.0",
    "tags": ["llm", "ai"],
    "input": "text",
    "output": "text",
}


def run(input_text: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[{"role": "user", "content": input_text}],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
