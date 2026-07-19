"""9P2000 server：把 Env 路徑樹以真線格式服務。

本檔＝編碼（qid/stat）＋連線迴圈＋監聽；訊息分派見 up_ninep_ops、編解碼見 up_ninep_codec。
"""
import socket
import struct
import threading

from up_env import Env
from up_ninep_codec import QTDIR, DMDIR, Rerror, p2, p4, p8, ps, Reader, ERRNO_STR
from up_ninep_ops import dispatch


class NineP:
    def __init__(self, env, msize=65536):
        self.env = env
        self.msize = msize
        self.qid_paths = {}     # 路徑字串 -> 穩定 qid.path 整數
        self.qid_next = 0

    def _isdir(self, path):
        return self.env.stat(path)["kind"] == "dir"

    def _qid(self, path):
        if path not in self.qid_paths:
            self.qid_paths[path] = self.qid_next
            self.qid_next += 1
        qtype = QTDIR if self._isdir(path) else 0
        return bytes([qtype]) + p4(0) + p8(self.qid_paths[path])

    @staticmethod
    def _name(path):
        parts = [p for p in path.split("/") if p]
        return parts[-1] if parts else "unipath"

    def _child_path(self, path, w):
        base = "" if path == "/" else path
        return base + "/" + w

    def _stat_bytes(self, path):
        st = self.env.stat(path)
        isdir = st["kind"] == "dir"
        mode = (DMDIR | 0o755) if isdir else (0o444 if st["ro"] else 0o644)
        length = 0 if isdir else st["size"]
        inner = (p2(0) + p4(0) + self._qid(path) + p4(mode)
                 + p4(0) + p4(0) + p8(length)
                 + ps(self._name(path)) + ps("unipath") + ps("unipath") + ps("unipath"))
        return p2(len(inner)) + inner

    def _err(self, name):
        return Rerror, ps(ERRNO_STR.get(name, name))

    def serve_conn(self, conn):
        fids = {}
        f = conn.makefile("rwb")
        while True:
            hdr = f.read(4)
            if len(hdr) < 4:
                break
            size = struct.unpack("<I", hdr)[0]
            body = f.read(size - 4)
            mtype, tag = body[0], struct.unpack_from("<H", body, 1)[0]
            r = Reader(body); r.i = 3
            try:
                rtype, rbody = dispatch(self, mtype, tag, r, fids)
            except Exception:
                rtype, rbody = Rerror, ps("i/o error")
            f.write(p4(7 + len(rbody)) + bytes([rtype]) + p2(tag) + rbody); f.flush()


def serve(port):
    np = NineP(Env())
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port)); srv.listen(8)
    print(f"unipath-9p 監聽 127.0.0.1:{port}（真 9P2000）", flush=True)
    while True:
        conn, _ = srv.accept()
        threading.Thread(target=np.serve_conn, args=(conn,), daemon=True).start()
