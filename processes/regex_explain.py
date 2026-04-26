#!/usr/bin/env python3
"""
Regex explainer — explain what a regex pattern does in plain language.
Input: regex pattern (optionally with sample input)
Output: plain language explanation + match breakdown
"""
import sys
import json
import os

METADATA = {
    "name": "regex_explain",
    "description": "Explain a regular expression pattern in plain language with examples",
    "version": "1.0",
    "tags": ["regex", "utility", "explain", "dev-tool"],
    "input": "regex pattern (optionally with sample string)",
    "output": "plain language explanation",
}

SYSTEM_PROMPT = """Explain the given regular expression clearly:
1. Overall: what does this regex match?
2. Break down each component (groups, quantifiers, anchors, character classes)
3. Give 2-3 example strings that match
4. Give 1-2 example strings that do NOT match
Be concise. Use a table for the breakdown if it has more than 4 parts."""


def run(pattern: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": pattern},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
