"""Example 01 — Basic usage.

Register a CalculateFunction and call it through the CoreService.
This example is a sanity-check that doesn't require an LLM API key.

Run:
    uv run python examples/01_basic_usage.py
"""

from ai_core import CalculateFunction, CoreService


def main() -> None:
    core = CoreService()

    # Register a simple uppercase function.
    core.register(CalculateFunction("upper_v1", lambda tokens: tokens.upper()))

    # Execute it.
    output, context = core.execute("upper_v1", b"hello world", context={})

    print("output :", output.decode())
    print("context:", context)
    print("functions:", core.list_functions())


if __name__ == "__main__":
    main()
