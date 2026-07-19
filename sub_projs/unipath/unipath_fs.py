#!/usr/bin/env python3
"""unipath step 1 — 假靜態樹（FUSE），證 mount 管線通。

樹與 ctl 邏輯見 up_static.py；FUSE 轉接見 up_fuse.py。
用法：./unipath_fs.py <mountpoint>；另一終端 ls/cat/echo，fusermount -u <mnt> 卸載。
"""
import sys

from fuse import FUSE

from up_fuse import FuseEnv
from up_static import StaticEnv

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"用法: {sys.argv[0]} <mountpoint>")
    FUSE(FuseEnv(StaticEnv()), sys.argv[1], foreground=True, nothreads=True)
