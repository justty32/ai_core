#!/usr/bin/env python3
"""
Multi-turn LLM chat process — maintains conversation history via session.
Each run reads history, appends new message, calls LLM, saves updated history.

Session file: ~/.ai_core/llm_chat/session.json
"""
import sys
import json
import os
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
from session_lib import Session


METADATA = {
    "name": "llm_chat",
    "description": "Multi-turn LLM conversation with persistent session history",
    "version": "1.0",
    "tags": ["llm", "chat", "multi-turn"],
    "input": "text",
    "output": "text",
}

SESSION_PATH = Path.home() / ".ai_core" / "llm_chat" / "session.json"


def run(input_text: str) -> str:
    import litellm

    session = Session(SESSION_PATH)
    history = session.get("history", [])
    system_prompt = os.environ.get("SYSTEM_PROMPT", "You are a helpful assistant.")
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")

    messages = [{"role": "system", "content": system_prompt}] + history
    messages.append({"role": "user", "content": input_text})

    response = litellm.completion(model=model, messages=messages)
    reply = response.choices[0].message.content

    history.append({"role": "user", "content": input_text})
    history.append({"role": "assistant", "content": reply})
    session.set("history", history)

    return reply


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    elif "--reset" in sys.argv:
        Session(SESSION_PATH).clear()
        print("Session cleared.")
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read().strip()
        print(run(text))
