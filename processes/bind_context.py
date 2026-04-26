#!/usr/bin/env python3
"""
Context-binding process — prepend a system prompt to input, then call llm_call.
System prompt is set via SYSTEM_PROMPT env var.

This demonstrates the 'bind' pattern: a specialized LLM with fixed context.
"""
import sys
import json
import os
import subprocess
from pathlib import Path


METADATA = {
    "name": "bind_context",
    "description": "Prepend a system prompt to input and call an LLM",
    "version": "1.0",
    "tags": ["llm", "context", "bind"],
    "input": "text",
    "output": "text",
}

PROCESSES_DIR = Path(__file__).parent


def run(input_text: str) -> str:
    system_prompt = os.environ.get("SYSTEM_PROMPT", "You are a helpful assistant.")
    combined = f"System: {system_prompt}\n\nUser: {input_text}"
    r = subprocess.run(
        [sys.executable, str(PROCESSES_DIR / "llm_call.py"), combined],
        capture_output=True, text=True
    )
    return r.stdout


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text), end="")
