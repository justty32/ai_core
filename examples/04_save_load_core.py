"""Example 04 — Persist and restore a Core.

Demonstrates that a Core can be saved to JSON and loaded back, with the
function registry intact.

Run:
    uv run python examples/04_save_load_core.py
"""

from pathlib import Path

from ai_core import CompositeFunction, CoreService, LLMFunction, ShellFunction
from ai_core.persistence.serializer import load_core, save_core


def main() -> None:
    out_path = Path("/tmp/ai_core_demo.json")

    # Build a core with mixed function types.
    core = CoreService()
    core.register(LLMFunction("chat_v1", model="claude-sonnet-4-6"))
    core.register(ShellFunction("echo_v1", "/home/lorkhan/repo/ai_core/scripts/shell/echo.sh"))
    core.register(
        CompositeFunction("pipe_v1", ["echo_v1", "chat_v1"], registry=core.registry)
    )

    # Save.
    save_core(core, str(out_path))
    print(f"saved to {out_path}")

    # Load back.
    restored = load_core(str(out_path))
    print(f"restored functions: {restored.list_functions()}")


if __name__ == "__main__":
    main()
