#!/usr/bin/env python3
"""
Go benchmark generator — generate benchmark functions for Go code.
Input: Go function to benchmark
Output: Go benchmark code
"""
import sys
import json
import os


METADATA = {
    "name": "go_bench",
    "description": "Generate Go benchmark functions (testing.B) for performance testing",
    "version": "1.0",
    "tags": ["go", "golang", "benchmark", "performance", "test"],
    "input": "go function source code",
    "output": "go benchmark functions",
}

SYSTEM_PROMPT = """You are an expert Go engineer.
Generate Go benchmark functions for the given code.
Requirements:
- Use testing.B correctly (b.N loop, b.ResetTimer where needed)
- Cover different input sizes if relevant (small, medium, large)
- Use b.ReportAllocs() to track allocations
- Add a parallel benchmark variant with b.RunParallel where applicable
- Output only the benchmark code"""


def run(code: str) -> str:
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    import litellm
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"```go\n{code}\n```"},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        code = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(code))
