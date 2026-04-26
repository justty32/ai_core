#!/usr/bin/env python3
"""
Text summarizer — summarize any text to key points.
Input: long text (article, doc, PR description, etc.)
Output: bullet-point summary
"""
import sys
import json
import os


METADATA = {
    "name": "summarize",
    "description": "Summarize text into concise bullet points",
    "version": "1.0",
    "tags": ["text", "summarize", "utility"],
    "input": "text",
    "output": "bullet-point summary",
}

SYSTEM_PROMPT = """Summarize the given text into concise bullet points.
- Extract only the key information
- Keep each bullet under 15 words
- 3-7 bullets for most inputs
- Output only the bullet points, no preamble"""


def run(text: str) -> str:
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    import litellm
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": text},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
