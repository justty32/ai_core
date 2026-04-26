#!/usr/bin/env python3
"""
Go commit message generator — generate a conventional commit message from a git diff.
Input: git diff output
Output: commit message (conventional commits format)
"""
import sys
import json
import os


METADATA = {
    "name": "go_commit_msg",
    "description": "Generate a conventional commit message from a git diff of Go code",
    "version": "1.0",
    "tags": ["go", "golang", "git", "commit", "workflow"],
    "input": "git diff output",
    "output": "conventional commit message",
}

SYSTEM_PROMPT = """You are an expert Go engineer.
Generate a concise git commit message for the given diff using Conventional Commits format:
  <type>(<scope>): <short description>

  [optional body]

Types: feat, fix, refactor, test, docs, chore, perf
- Keep the subject line under 72 characters
- Use imperative mood ("add" not "added")
- Body explains WHY, not what (the diff shows what)
- Output only the commit message, nothing else"""


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
