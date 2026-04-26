#!/usr/bin/env python3
"""
General Q&A — ask any question and get a direct, expert answer.
Maintains conversation history for follow-up questions.
"""
import sys
import json
import os
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))
from session_lib import Session

METADATA = {
    "name": "ask",
    "description": "General Q&A with persistent conversation history for follow-up questions",
    "version": "1.0",
    "tags": ["qa", "chat", "utility", "general"],
    "input": "question",
    "output": "answer",
}

SESSION_PATH = Path.home() / ".ai_core" / "ask" / "session.json"

SYSTEM_PROMPT = """You are a knowledgeable expert.
Answer questions directly and concisely.
- Lead with the answer, not the explanation
- Use concrete examples when helpful
- Admit uncertainty rather than guessing"""


def run(question: str) -> str:
    import litellm
    session = Session(SESSION_PATH)
    history = session.get("history", [])
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")

    messages = [{"role": "system", "content": SYSTEM_PROMPT}] + history
    messages.append({"role": "user", "content": question})

    response = litellm.completion(model=model, messages=messages)
    reply = response.choices[0].message.content

    history.append({"role": "user", "content": question})
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
