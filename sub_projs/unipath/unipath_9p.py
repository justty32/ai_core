#!/usr/bin/env python3
"""unipath step 4 — 真 9P2000 server 入口（可被核心 v9fs mount）。

實作拆分：up_ninep（server＋編碼）/ up_ninep_ops（訊息分派）/ up_ninep_codec（編解碼）/
up_ninep_client（獨立真 9P client 自測）。

用法：
  server ： ./unipath_9p.py serve 5640
  自測   ： ./unipath_9p.py selftest 5640
  核心掛 ： sudo modprobe 9pnet_fd 9p
           sudo mount -t 9p -o trans=tcp,port=5640,version=9p2000,uname=me 127.0.0.1 mnt
"""
import sys

from up_ninep import serve
from up_ninep_client import selftest

if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("serve", "selftest"):
        sys.exit(f"用法: {sys.argv[0]} serve|selftest <port>")
    (serve if sys.argv[1] == "serve" else selftest)(int(sys.argv[2]))
