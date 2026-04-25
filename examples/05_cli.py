"""Example 05 — Simple CLI client.

A minimal interactive REPL that talks to a running ai_core server.
Demonstrates the "client owns the context" model: this script keeps
its own context dict and threads it through each request.

Usage:
    # 1. In one terminal, start the server:
    uv run python examples/03_run_server.py

    # 2. In another terminal, run this CLI:
    uv run python examples/05_cli.py
    # or, point at a different server:
    uv run python examples/05_cli.py --url http://localhost:9000

REPL commands:
    /help                 Show this help
    /functions            List functions available on the server
    /info <func_id>       Show metadata for a function
    /use  <func_id>       Set the default function for plain input
    /context              Show the current local context
    /clear                Reset the local context to {}
    /save <path>          Write the local context to a JSON file
    /load <path>          Replace the local context from a JSON file
    /exit  (or /quit)     Leave

Anything not starting with `/` is sent as input to the default function.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

import httpx

from ai_core.service.client import HTTPClient

PROMPT = "ai_core> "


# --- Per-command handlers ------------------------------------------------

def cmd_help(_arg: str, _state: dict, _client: HTTPClient) -> None:
    print(__doc__)


def cmd_functions(_arg: str, _state: dict, client: HTTPClient) -> None:
    try:
        names = client.list_functions()
    except httpx.HTTPError as e:
        print(f"error: {e}")
        return
    if not names:
        print("(no functions registered)")
        return
    for n in names:
        print(f"  {n}")


def cmd_info(arg: str, _state: dict, client: HTTPClient) -> None:
    if not arg:
        print("usage: /info <func_id>")
        return
    try:
        info = client.get_function_info(arg)
    except httpx.HTTPError as e:
        print(f"error: {e}")
        return
    print(json.dumps(info, indent=2, ensure_ascii=False))


def cmd_use(arg: str, state: dict, _client: HTTPClient) -> None:
    if not arg:
        print("usage: /use <func_id>")
        return
    state["default_func"] = arg
    print(f"default function set to: {arg}")


def cmd_context(_arg: str, state: dict, _client: HTTPClient) -> None:
    print(json.dumps(state["context"], indent=2, ensure_ascii=False))


def cmd_clear(_arg: str, state: dict, _client: HTTPClient) -> None:
    state["context"] = {}
    print("context cleared")


def cmd_save(arg: str, state: dict, _client: HTTPClient) -> None:
    if not arg:
        print("usage: /save <path>")
        return
    path = Path(arg).expanduser()
    path.write_text(json.dumps(state["context"], indent=2, ensure_ascii=False))
    print(f"context saved to {path}")


def cmd_load(arg: str, state: dict, _client: HTTPClient) -> None:
    if not arg:
        print("usage: /load <path>")
        return
    path = Path(arg).expanduser()
    if not path.exists():
        print(f"file not found: {path}")
        return
    state["context"] = json.loads(path.read_text())
    print(f"context loaded from {path}")


def cmd_exit(_arg: str, _state: dict, _client: HTTPClient) -> None:
    raise SystemExit(0)


COMMANDS = {
    "help": cmd_help,
    "functions": cmd_functions,
    "info": cmd_info,
    "use": cmd_use,
    "context": cmd_context,
    "clear": cmd_clear,
    "save": cmd_save,
    "load": cmd_load,
    "exit": cmd_exit,
    "quit": cmd_exit,
}


# --- Main loop ------------------------------------------------------------

def call_default_function(line: str, state: dict, client: HTTPClient) -> None:
    """Send `line` to the currently-selected default function."""
    func_id = state.get("default_func")
    if func_id is None:
        print("no default function set. try: /functions, then /use <id>")
        return

    try:
        output, state["context"] = client.execute(
            func_id,
            line.encode("utf-8"),
            state["context"],
        )
    except httpx.HTTPStatusError as e:
        # Surface the server's structured error if it returned one.
        try:
            body = e.response.json()
            msg = body.get("detail") or body.get("error") or str(e)
        except (ValueError, AttributeError):
            msg = str(e)
        print(f"error ({e.response.status_code}): {msg}")
        return
    except httpx.HTTPError as e:
        print(f"error: {e}")
        return

    print(output.decode("utf-8", errors="replace"))


def dispatch(line: str, state: dict, client: HTTPClient) -> None:
    """Route a line to either a slash command or the default function."""
    if line.startswith("/"):
        parts = line[1:].split(maxsplit=1)
        cmd = parts[0]
        arg = parts[1] if len(parts) > 1 else ""
        handler = COMMANDS.get(cmd)
        if handler is None:
            print(f"unknown command: /{cmd}. type /help")
            return
        handler(arg, state, client)
        return
    call_default_function(line, state, client)


def repl(client: HTTPClient) -> None:
    state: dict[str, Any] = {
        "default_func": None,
        "context": {},
    }
    print(f"connected to {client.base_url}. type /help for commands, /exit to quit.")
    while True:
        try:
            line = input(PROMPT).strip()
        except EOFError:
            print()
            return
        except KeyboardInterrupt:
            print()  # newline after ^C, then keep going
            continue
        if not line:
            continue
        try:
            dispatch(line, state, client)
        except SystemExit:
            return


def main() -> None:
    parser = argparse.ArgumentParser(description="ai_core CLI client")
    parser.add_argument(
        "--url",
        default="http://localhost:8000",
        help="Core server URL (default: %(default)s)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=60.0,
        help="Per-request timeout in seconds (default: %(default)s)",
    )
    args = parser.parse_args()

    client = HTTPClient(base_url=args.url, timeout=args.timeout)
    try:
        repl(client)
    except KeyboardInterrupt:
        sys.exit(0)


if __name__ == "__main__":
    main()
