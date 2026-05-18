#!/usr/bin/env python3
"""Convert text into a URL-safe slug: lowercase, ASCII, hyphen-separated."""
import sys
import json
import re
import unicodedata


METADATA = {
    "name": "slugify",
    "description": "Convert text into a URL-safe slug (lowercase ASCII, hyphen-separated)",
    "version": "1.0",
    "tags": ["text", "url", "slug", "utility"],
    "input": "text",
    "output": "slug",
}


def run(text: str) -> str:
    text = unicodedata.normalize("NFKD", text)
    text = text.encode("ascii", "ignore").decode("ascii")
    text = text.lower()
    text = re.sub(r"[^a-z0-9]+", "-", text)
    return text.strip("-")


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
