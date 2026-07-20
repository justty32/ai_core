#!/usr/bin/env python3
"""main.py — Python binding CLI 的進入點：`python cli/main.py [旗標...] [prompt...]`。

薄殼，等同 core-py 的 cllm/__main__.py：把控制權交給 cli._entry（含 KeyboardInterrupt → 130）。
以腳本方式跑（非 package），故同目錄姊妹模組用扁平 import；`import llm` 靠 env.sh 設好的
PYTHONPATH（指向 ctypes binding llm.py）。先 `source ~/dev/cllm/env.sh`。
"""
import os
import sys

# 以腳本跑時 sys.path[0] 已是本檔所在的 cli/；保險起見顯式補上，讓姊妹模組扁平 import 穩定。
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from cli import _entry

if __name__ == "__main__":
    _entry()
