#!/usr/bin/env python3
"""example.py — cllm Python binding 示範。
跑法（純 Python，先建好 ../../build/libcllm.so）：
    python3 example.py "file:///<cllm絕對路徑>/test/fixtures/"   # 離線走 fixture
    python3 example.py                                            # 無參數 → 內建 localhost（要真後端）
libcllm.so 路徑預設 ../../build/libcllm.so，可用環境變數 LIBCLLM 覆寫。
"""
import sys
import llm

base = sys.argv[1] if len(sys.argv) > 1 else ""

# ① 位置形式：prompt + endpoint（base 為空 → 省略 endpoint → 內建 localhost）
if base:
    ans = llm.ask("你好", base + "fake/chat/completions")
else:
    ans = llm.ask("你好")
print("[py] llm.ask =>", ans)

# ② 串流 on_delta（逐段即時印；回真值可中止）
if base:
    print("[py] 串流逐段 => ", end="")
    whole = llm.ask("數到五",
                    endpoint=base + "fake_stream/chat/completions",
                    stream=True,
                    on_delta=lambda piece: (print("[" + piece + "]", end="", flush=True), False)[1])
    print("　合＝" + whole)

# ③ schema 結構化輸出
if base:
    j = llm.ask("給我角色",
                endpoint=base + "fake_json/chat/completions",
                schema='{"type":"object"}')
    print("[py] schema =>", j)
