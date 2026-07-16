#!/usr/bin/env python3
"""example.py — cllm Python binding：基本 ask、串流、schema+JSON 解析、shell(CLI) 呼叫。
跑：source ~/repo/dev/env.sh 後  python3 example.py "$CLLM_FIXTURES" """
import sys, json, subprocess
import llm
base = sys.argv[1] if len(sys.argv) > 1 else ""
ep = lambda n: (base + n) if base else None

print("[py] ask =>", llm.ask("你好", ep("fake/chat/completions")))
print("[py] 串流 => ", end="")
llm.ask("數到五", endpoint=ep("fake_stream/chat/completions"), stream=True,
        on_delta=lambda p: (print(f"[{p}]", end="", flush=True), False)[1]); print()

# schema → JSON → stdlib json 解析
obj = json.loads(llm.ask("給我角色", endpoint=ep("fake_json/chat/completions"), schema='{"type":"object"}'))
print(f"[py] json => name={obj['name']} affection={obj['affection']} lines={len(obj['lines'])}")

# shell 呼叫：從 Python 呼叫 llm CLI，捕捉答案
out = subprocess.run(["llm", "你好", "--endpoint", ep("fake/chat/completions")],
                     capture_output=True, text=True)
print("[py] shell(llm) =>", out.stdout.strip())
