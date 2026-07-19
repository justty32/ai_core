#!/usr/bin/env python3
"""unipath step 3 — 發布端：一個活 process 把自己的執行態環境掛到 Unix socket 上。

這是「從外部定址另一個 process」的被定址方。它持有活 Env（會被背景 tick 變動），
在 Unix socket 上接受 **9P 形狀的 RPC**（stat/readdir/read/write），供掛載端
（unipath_mount.py）遠端 walk/讀/寫。

界線：這是 9P 的**形狀**（walk/open/read/write 那組動詞），**還不是真 9P 線格式**——
先用 newline-delimited JSON 把跨 process 這件事做通，日後平滑轉真 9P（見 AGENTS.md）。

協議（每行一個 JSON 請求 → 每行一個 JSON 回應；data 用 base64 保二進位安全）：
  {"op":"stat","path":"/0"}          -> {"ok":true,"kind":"dir"|"file","size":N,"ro":bool}
  {"op":"readdir","path":"/"}        -> {"ok":true,"entries":[...]}
  {"op":"read","path":"/0/data"}     -> {"ok":true,"data":"<base64>"}
  {"op":"write","path":"/0/data","data":"<base64>"} -> {"ok":true,"n":N}
  失敗                                -> {"ok":false,"errno":"ENOENT"}
"""
import base64
import json
import os
import socket
import sys
import threading

from unipath_env import Env, UnipathError


def handle(env, conn):
    f = conn.makefile("rwb")
    for raw in f:
        try:
            req = json.loads(raw)
            op = req.get("op")
            if op == "stat":
                resp = {"ok": True, **env.stat(req["path"])}
            elif op == "readdir":
                resp = {"ok": True, "entries": env.readdir(req["path"])}
            elif op == "read":
                resp = {"ok": True,
                        "data": base64.b64encode(env.read(req["path"])).decode()}
            elif op == "write":
                data = base64.b64decode(req["data"])
                resp = {"ok": True, "n": env.write(req["path"], data)}
            else:
                resp = {"ok": False, "errno": "EINVAL"}
        except UnipathError as e:
            resp = {"ok": False, "errno": e.name}
        except Exception:                       # 協議/解析錯：回 EIO，別讓服務掛掉
            resp = {"ok": False, "errno": "EIO"}
        f.write((json.dumps(resp) + "\n").encode())
        f.flush()


def main(sock_path):
    if os.path.exists(sock_path):
        os.unlink(sock_path)
    env = Env()  # 帶背景 tick 的活環境
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(sock_path)
    srv.listen(8)
    print(f"unipath-pub 監聽 {sock_path}（Ctrl-C 停）", flush=True)
    try:
        while True:
            conn, _ = srv.accept()
            threading.Thread(target=handle, args=(env, conn), daemon=True).start()
    except KeyboardInterrupt:
        pass
    finally:
        srv.close()
        os.unlink(sock_path)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"用法: {sys.argv[0]} <socket 路徑>")
    main(sys.argv[1])
