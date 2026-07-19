#!/usr/bin/env python3
"""unipath step 2 — 真執行態環境（自掛）：把本 process 的活 Env 經 FUSE 掛成路徑樹。

背景 tick 讓 /0 counter 隨時間變（執行態非快照）；echo > …/data write-through 改活物件。
env 邏輯見 up_env.py；FUSE 轉接見 up_fuse.py。用法同 step 1（換本檔）。
"""
import sys

from fuse import FUSE

from up_env import Env
from up_fuse import FuseEnv

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"用法: {sys.argv[0]} <mountpoint>")
    FUSE(FuseEnv(Env()), sys.argv[1], foreground=True, nothreads=True)
