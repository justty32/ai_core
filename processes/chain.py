#!/usr/bin/env python3
"""
Chain — execute a planner-produced JSON plan, running each process in order.
Input: JSON plan from planner + initial input (via --input or stdin for plan)
Output: final step's output, with intermediate results saved to session

Usage:
  python planner.py "review and fix my go code" | python chain.py --input myfile.go
  python chain.py --plan plan.json --input myfile.go
"""
import sys
import json
import os
import subprocess
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
from session_lib import Session

METADATA = {
    "name": "chain",
    "description": "Execute a planner JSON plan: run each process in order, pipe outputs between steps",
    "version": "1.0",
    "tags": ["ai", "orchestration", "chain", "meta"],
    "input": "json plan (from planner) + --input <initial_input>",
    "output": "final step output",
}

LIST_FILE = Path(__file__).parent.parent / "list.json"
SESSION_PATH = Path.home() / ".ai_core" / "chain" / "session.json"


def find_process_path(name: str) -> str | None:
    if not LIST_FILE.exists():
        return None
    entries = json.loads(LIST_FILE.read_text())
    for e in entries:
        if e["name"] == name:
            return e["path"]
    return None


def run_step(process_path: str, input_text: str) -> str:
    r = subprocess.run(
        [sys.executable, process_path],
        input=input_text,
        capture_output=True,
        text=True,
        timeout=60,
    )
    if r.returncode != 0:
        raise RuntimeError(f"Process {process_path} failed: {r.stderr.strip()}")
    return r.stdout


def run(plan_json: str, initial_input: str) -> str:
    plan = json.loads(plan_json)
    if isinstance(plan, dict) and "error" in plan:
        return f"Plan error: {plan['error']}"

    session = Session(SESSION_PATH)
    results = {"0": initial_input}  # step 0 = initial input

    for step in plan:
        step_num = str(step["step"])
        process_name = step["process"]
        depends_on = str(step.get("depends_on") or 0)

        process_path = find_process_path(process_name)
        if not process_path:
            print(f"[chain] WARNING: process '{process_name}' not found, skipping", file=sys.stderr)
            continue

        input_text = results.get(depends_on, initial_input)
        print(f"[chain] step {step_num}: {process_name} ({step.get('reason', '')})", file=sys.stderr)

        output = run_step(process_path, input_text)
        results[step_num] = output
        session.set(f"step_{step_num}", {"process": process_name, "output": output})

    last_step = str(max(int(k) for k in results if k != "0"))
    return results[last_step]


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
        sys.exit(0)

    args = sys.argv[1:]
    initial_input = ""
    plan_json = ""

    if "--input" in args:
        idx = args.index("--input")
        input_arg = args[idx + 1]
        initial_input = Path(input_arg).read_text() if Path(input_arg).exists() else input_arg

    if "--plan" in args:
        idx = args.index("--plan")
        plan_json = Path(args[idx + 1]).read_text()
    else:
        plan_json = sys.stdin.read().strip()

    print(run(plan_json, initial_input))
