#!/usr/bin/env python3
"""Echo input back to stdout — simplest possible process, for testing."""
import sys
import json


METADATA = {
    "name": "echo",
    "description": "Echo input text back to stdout unchanged",
    "version": "1.0",
    "tags": ["utility", "debug"],
    "input": "text",
    "output": "text",
    "examples": [
        {"input": "hello", "output": "hello"},
        {"input": "", "output": ""},
    ],
}


def run(input_text: str) -> str:
    return input_text


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text), end="")
