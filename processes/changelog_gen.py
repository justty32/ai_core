#!/usr/bin/env python3
"""
Changelog generator — generate CHANGELOG entry from git log.
Input: git log output (e.g. git log --oneline v1.0..HEAD)
Output: formatted changelog section
"""
import sys
import json
import os

METADATA = {
    "name": "changelog_gen",
    "description": "Generate a CHANGELOG.md entry from git log commits",
    "version": "1.0",
    "tags": ["git", "changelog", "release", "workflow"],
    "input": "git log output",
    "output": "changelog markdown section",
}

SYSTEM_PROMPT = """You are a technical writer.
Generate a CHANGELOG.md entry from the given git log.
Format:
## [version] - YYYY-MM-DD

### Added
- ...

### Changed
- ...

### Fixed
- ...

### Removed
- ...

Rules:
- Group commits by type (feat→Added, fix→Fixed, refactor→Changed, etc.)
- Write from the user's perspective ("Add X" not "Added X to src/")
- Skip chore/docs/test commits unless they affect users
- Use [version] as placeholder if no version is given
- Output only the markdown"""


def run(log: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": log},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
