#!/usr/bin/env python3
"""
Go channel pattern generator — generate common concurrency patterns using channels.
Input: pattern name or description (pipeline, fan-out, fan-in, worker pool, etc.)
"""
import sys
import json
import os

METADATA = {
    "name": "go_channel_patterns",
    "description": "Generate Go concurrency patterns: pipeline, fan-out/in, worker pool, timeout, done channel",
    "version": "1.0",
    "tags": ["go", "golang", "channel", "concurrency", "goroutine", "patterns"],
    "input": "pattern name or description (e.g. 'worker pool with 5 workers')",
    "output": "go concurrency pattern implementation",
}

SYSTEM_PROMPT = """You are an expert Go concurrency engineer.
Generate the requested Go channel/concurrency pattern.
Common patterns: pipeline, fan-out, fan-in, worker pool, done channel, timeout, semaphore, rate limiter.

Requirements:
- Clean shutdown: all goroutines must be able to exit
- Use done channel or context for cancellation
- No goroutine leaks
- Add comments explaining the pattern's data flow
- Include a simple main() or example showing usage"""


def run(description: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": description},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        desc = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(desc))
