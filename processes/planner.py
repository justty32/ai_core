#!/usr/bin/env python3
"""
Planner — decompose a natural language task into a sequence of process calls.
Input: task description
Output: JSON execution plan [{process, input_note, depends_on}]

Uses hub's list.json to know what processes are available.
"""
import sys
import json
import os
from pathlib import Path

METADATA = {
    "name": "planner",
    "description": "Decompose a natural language task into an ordered sequence of process calls",
    "version": "1.0",
    "tags": ["ai", "orchestration", "planner", "meta"],
    "input": "task description in natural language",
    "output": "json execution plan",
}

LIST_FILE = Path(__file__).parent.parent / "list.json"

SYSTEM_PROMPT = """You are an AI orchestration planner.
Given a task and a list of available processes, output a JSON execution plan.

Output format (JSON array, nothing else):
[
  {
    "step": 1,
    "process": "<process_name>",
    "input_note": "<what to pass as input>",
    "depends_on": <step number or null>,
    "reason": "<why this step>"
  }
]

Rules:
- Only use processes from the available list
- Order steps so dependencies come first
- Keep the plan minimal (don't add unnecessary steps)
- If a task can't be done with available processes, output {"error": "reason"}"""


def run(task: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")

    processes = []
    if LIST_FILE.exists():
        entries = json.loads(LIST_FILE.read_text())
        processes = [{"name": e["name"], "description": e["description"], "tags": e.get("tags", [])} for e in entries]

    user_msg = f"Task: {task}\n\nAvailable processes:\n{json.dumps(processes, indent=2)}"
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": user_msg},
        ],
    )
    raw = response.choices[0].message.content.strip()
    # strip markdown code fences if present
    if raw.startswith("```"):
        raw = "\n".join(raw.split("\n")[1:])
        if raw.endswith("```"):
            raw = raw[: raw.rfind("```")]
    return raw.strip()


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        task = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read().strip()
        print(run(task))
