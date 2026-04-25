"""Example 02 — Composite function.

Build a 3-step pipeline using CompositeFunction. No LLM needed.

Pipeline:
    bytes -> reverse -> upper -> tag -> bytes

Run:
    uv run python examples/02_composite.py
"""

from ai_core import CalculateFunction, CompositeFunction, CoreService


def main() -> None:
    core = CoreService()

    core.register(CalculateFunction("reverse_v1", lambda b: b[::-1]))
    core.register(CalculateFunction("upper_v1", lambda b: b.upper()))
    core.register(CalculateFunction("tag_v1", lambda b: b"[" + b + b"]"))

    pipe = CompositeFunction(
        "pipe_v1",
        ["reverse_v1", "upper_v1", "tag_v1"],
        registry=core.registry,
    )
    core.register(pipe)

    output, _ = core.execute("pipe_v1", b"hello", context={})
    print("output:", output.decode())  # expected: [OLLEH]


if __name__ == "__main__":
    main()
