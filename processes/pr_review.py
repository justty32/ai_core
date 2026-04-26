#!/usr/bin/env python3
"""
PR / diff reviewer — review a git diff as a code reviewer.
Input: git diff or patch
Output: review comments by file/section
"""
import sys
import json
import os


METADATA = {
    "name": "pr_review",
    "description": "Review a git diff as a senior engineer — logic, safety, style",
    "version": "1.0",
    "tags": ["git", "pr", "review", "diff", "workflow"],
    "input": "git diff or patch",
    "output": "review comments",
}

SYSTEM_PROMPT = """You are a senior engineer reviewing a pull request.
Review the diff for:
- Logic errors or off-by-one bugs
- Security issues (injection, auth, data exposure)
- Missing error handling
- Race conditions or concurrency issues
- Naming and readability
- Missing tests for changed behavior

Format: group comments by file. Use "BLOCKING:" for must-fix issues,
"SUGGESTION:" for improvements, "QUESTION:" for clarifications needed.
Be direct and specific. Skip nitpicks."""


def run(diff: str) -> str:
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    import litellm
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": diff},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        diff = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(diff))
