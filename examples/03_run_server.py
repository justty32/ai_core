"""Example 03 — Run the FastAPI server.

Spins up a CoreService with a couple of demo functions and serves it
over HTTP.

Run:
    uv run python examples/03_run_server.py

Then in another shell:
    curl -X POST http://localhost:8000/execute \\
        -H 'Content-Type: application/json' \\
        -d '{"func_id":"upper_v1","tokens":"aGVsbG8=","context":{}}'

(`aGVsbG8=` is base64 for "hello".)
"""

import uvicorn

from ai_core import CalculateFunction, CoreService, create_app


def build_core() -> CoreService:
    core = CoreService()
    core.register(CalculateFunction("upper_v1", lambda b: b.upper()))
    core.register(CalculateFunction("reverse_v1", lambda b: b[::-1]))
    return core


def main() -> None:
    core = build_core()
    app = create_app(core)
    uvicorn.run(app, host="127.0.0.1", port=8000)


if __name__ == "__main__":
    main()
