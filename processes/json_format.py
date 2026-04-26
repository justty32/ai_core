#!/usr/bin/env python3
"""
JSON formatter and validator — pretty-print and validate JSON.
Input: JSON string (possibly minified or malformed)
Output: pretty-printed JSON or error message
"""
import sys
import json

METADATA = {
    "name": "json_format",
    "description": "Validate and pretty-print JSON with 2-space indentation",
    "version": "1.0",
    "tags": ["json", "format", "utility", "dev-tool"],
    "input": "json string",
    "output": "formatted json or error",
}


def run(text: str) -> str:
    text = text.strip()
    try:
        parsed = json.loads(text)
        return json.dumps(parsed, ensure_ascii=False, indent=2)
    except json.JSONDecodeError as e:
        return f"JSON error: {e}"


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
