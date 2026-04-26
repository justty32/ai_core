#!/usr/bin/env python3
"""
SQL explainer and reviewer — explain SQL queries and suggest improvements.
Input: SQL query
Output: explanation + performance/correctness suggestions
"""
import sys
import json
import os

METADATA = {
    "name": "sql_explain",
    "description": "Explain SQL queries and suggest index/performance improvements",
    "version": "1.0",
    "tags": ["sql", "database", "explain", "performance", "postgres"],
    "input": "sql query",
    "output": "explanation and improvement suggestions",
}

SYSTEM_PROMPT = """You are an expert database engineer (PostgreSQL focus).
For the given SQL query:
1. Explain what it does in plain language
2. Identify potential performance issues (missing indexes, N+1, full scans)
3. Suggest improvements (rewrite, index hint, CTE vs subquery)
4. Note any correctness concerns (NULL handling, implicit type casts)

Be specific. Reference the actual table/column names from the query."""


def run(sql: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": sql},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
