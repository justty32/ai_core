#!/usr/bin/env python3
"""
Translate to Traditional Chinese — translate any text to 繁體中文.
Input: text in any language
Output: Traditional Chinese translation
"""
import sys
import json
import os


METADATA = {
    "name": "translate_zh",
    "description": "Translate text to Traditional Chinese (繁體中文)",
    "version": "1.0",
    "tags": ["translate", "chinese", "language", "utility"],
    "input": "text in any language",
    "output": "Traditional Chinese text",
}

SYSTEM_PROMPT = """Translate the given text to Traditional Chinese (繁體中文).
- Use natural, fluent Traditional Chinese
- Preserve technical terms where appropriate (e.g. keep English for code/API names)
- Output only the translation, no explanation"""


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
