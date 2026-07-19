#!/usr/bin/env python3
"""unipath step 3 — 掛載端：薄 FUSE 客戶端，把另一個 process 的環境掛成本機路徑樹。

它自己**不持有任何 world 狀態**——每個 FUSE 操作都轉成一次 9P 形狀的 RPC，
打到發布端（unipath_pub.py）的 Unix socket。於是 `ls /mnt/0`、`cat /mnt/0/data`
讀到的，是**另一個 process** 的執行態環境（且隨其 tick 即時變）。

用法：
  終端 A： ./unipath_pub.py /tmp/unipath.sock
  終端 B： ./unipath_mount.py /tmp/unipath.sock mnt
  終端 C： ls mnt / cat mnt/0/data / echo 5 > mnt/2/hp/data ; fusermount -u mnt
"""
import base64
import errno
import json
import socket
import stat
import sys

from fuse import FUSE, Operations, FuseOSError


class UnipathClient(Operations):
    def __init__(self, sock_path):
        self.sock_path = sock_path

    def _rpc(self, **req):
        """一次一連線的 9P 形狀 RPC（單執行緒 FUSE，連線成本可忽略）。"""
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            s.connect(self.sock_path)
            s.sendall((json.dumps(req) + "\n").encode())
            buf = b""
            while not buf.endswith(b"\n"):
                chunk = s.recv(65536)
                if not chunk:
                    break
                buf += chunk
        finally:
            s.close()
        resp = json.loads(buf)
        if not resp.get("ok"):
            raise FuseOSError(getattr(errno, resp.get("errno", "EIO"), errno.EIO))
        return resp

    def getattr(self, path, fh=None):
        r = self._rpc(op="stat", path=path)
        if r["kind"] == "dir":
            return dict(st_mode=stat.S_IFDIR | 0o755, st_nlink=2)
        mode = 0o444 if r["ro"] else 0o644
        return dict(st_mode=stat.S_IFREG | mode, st_nlink=1, st_size=r["size"])

    def readdir(self, path, fh):
        return [".", ".."] + self._rpc(op="readdir", path=path)["entries"]

    def open(self, path, flags):
        self._rpc(op="stat", path=path)   # 存在性檢查
        return 0

    def read(self, path, size, offset, fh):
        data = base64.b64decode(self._rpc(op="read", path=path)["data"])
        return data[offset:offset + size]

    def truncate(self, path, length, fh=None):
        return 0

    def write(self, path, data, offset, fh):
        r = self._rpc(op="write", path=path, data=base64.b64encode(data).decode())
        return r["n"]


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(f"用法: {sys.argv[0]} <socket 路徑> <mountpoint>")
    FUSE(UnipathClient(sys.argv[1]), sys.argv[2], foreground=True, nothreads=True)
