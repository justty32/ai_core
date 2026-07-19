#!/usr/bin/env python3
"""unipath step 4 — 真 9P2000 線格式 server（可被核心 v9fs mount）。

step 3 是 9P 的「形狀」（JSON-over-socket）；本步是**真 9P2000 逐位元組線格式**，
於是可被 Linux 核心 v9fs 直接掛載，脫離 FUSE，並具網路透明（跨機器＝兩世界通聯）。

暴露的樹沿用 unipath_env.Env（活執行態環境；list→數字、dict→字串鍵、
每節點含 data/ctl/status）。

用法：
  server ： ./unipath_9p.py serve 5640
  自測   ： ./unipath_9p.py selftest 5640   （獨立真 9P client 驗線格式互通）
  核心掛 ： sudo modprobe 9pnet_fd 9p
           sudo mount -t 9p -o trans=tcp,port=5640,version=9p2000,uname=me 127.0.0.1 mnt

9P2000 訊息：size[4] type[1] tag[2] body。整數 little-endian；字串 size[2]+utf8。
"""
import socket
import struct
import sys
import threading

from unipath_env import Env, UnipathError

# 訊息型別
Tversion, Rversion = 100, 101
Tattach, Rattach = 104, 105
Rerror = 107
Tflush, Rflush = 108, 109
Twalk, Rwalk = 110, 111
Topen, Ropen = 112, 113
Tcreate, Rcreate = 114, 115
Tread, Rread = 116, 117
Twrite, Rwrite = 118, 119
Tclunk, Rclunk = 120, 121
Tremove, Rremove = 122, 123
Tstat, Rstat = 124, 125
Twstat, Rwstat = 126, 127

QTDIR = 0x80
DMDIR = 0x80000000
NOFID = 0xFFFFFFFF


# ---- 編解碼小工具 ----
def p2(v): return struct.pack("<H", v)
def p4(v): return struct.pack("<I", v)
def p8(v): return struct.pack("<Q", v)
def ps(s):
    b = s.encode()
    return p2(len(b)) + b


class Reader:
    def __init__(self, buf): self.b, self.i = buf, 0
    def u1(self): v = self.b[self.i]; self.i += 1; return v
    def u2(self): v = struct.unpack_from("<H", self.b, self.i)[0]; self.i += 2; return v
    def u4(self): v = struct.unpack_from("<I", self.b, self.i)[0]; self.i += 4; return v
    def u8(self): v = struct.unpack_from("<Q", self.b, self.i)[0]; self.i += 8; return v
    def s(self):
        n = self.u2(); v = self.b[self.i:self.i + n].decode(); self.i += n; return v


ERRNO_STR = {"ENOENT": "no such file", "ENOTDIR": "not a directory",
             "EISDIR": "is a directory", "EROFS": "read-only", "EIO": "i/o error"}


class NineP:
    """一個 9P2000 server：把 Env 的路徑樹以真線格式服務。"""

    def __init__(self, env, msize=65536):
        self.env = env
        self.msize = msize
        self.qid_paths = {}     # 路徑字串 -> 穩定 qid.path 整數
        self.qid_next = 0

    # ---- 路徑 / qid ----
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

    # ---- 訊息分派 ----
    def dispatch(self, mtype, tag, r, fids):
        if mtype == Tversion:
            ms = r.u4(); ver = r.s()
            self.msize = min(self.msize, ms)
            v = "9P2000" if ver.startswith("9P2000") else "unknown"
            return Rversion, p4(self.msize) + ps(v)

        if mtype == Tattach:
            fid = r.u4(); r.u4(); r.s(); r.s()   # fid afid uname aname
            fids[fid] = {"path": "/", "open": False}
            return Rattach, self._qid("/")

        if mtype == Twalk:
            fid = r.u4(); newfid = r.u4(); n = r.u2()
            names = [r.s() for _ in range(n)]
            if fid not in fids:
                return self._err("ENOENT")
            path = fids[fid]["path"]
            qids = []
            for w in names:
                cand = "/" if w == ".." else self._child_path(path, w)
                try:
                    self.env.stat(cand)
                except UnipathError:
                    break
                path = cand
                qids.append(self._qid(path))
            if len(qids) != n and not qids:
                return self._err("ENOENT")
            if len(qids) == n:
                fids[newfid] = {"path": path, "open": False}
            return Rwalk, p2(len(qids)) + b"".join(qids)

        if mtype == Topen:
            fid = r.u4(); r.u1()
            st = fids.get(fid)
            if not st:
                return self._err("ENOENT")
            st["open"] = True
            if self._isdir(st["path"]):
                st["dir"] = [self._stat_bytes(self._child_path(st["path"], c))
                             for c in self.env.readdir(st["path"])]
            return Ropen, self._qid(st["path"]) + p4(0)

        if mtype == Tread:
            fid = r.u4(); off = r.u8(); count = r.u4()
            st = fids.get(fid)
            if not st:
                return self._err("ENOENT")
            if "dir" in st:
                out, pos = b"", 0
                for e in st["dir"]:
                    if pos < off:
                        pos += len(e); continue
                    if len(out) + len(e) > count:
                        break
                    out += e; pos += len(e)
                return Rread, p4(len(out)) + out
            try:
                data = self.env.read(st["path"])
            except UnipathError as e:
                return self._err(e.name)
            chunk = data[off:off + count]
            return Rread, p4(len(chunk)) + chunk

        if mtype == Twrite:
            fid = r.u4(); r.u8(); count = r.u4(); data = r.b[r.i:r.i + count]
            st = fids.get(fid)
            if not st:
                return self._err("ENOENT")
            try:
                n = self.env.write(st["path"], data)
            except UnipathError as e:
                return self._err(e.name)
            return Rwrite, p4(n)

        if mtype == Tstat:
            fid = r.u4()
            st = fids.get(fid)
            if not st:
                return self._err("ENOENT")
            sb = self._stat_bytes(st["path"])
            return Rstat, p2(len(sb)) + sb

        if mtype == Tclunk:
            fid = r.u4(); fids.pop(fid, None)
            return Rclunk, b""

        if mtype == Tflush:
            r.u2()
            return Rflush, b""

        if mtype in (Tcreate, Tremove, Twstat):
            return self._err("EROFS")

        return self._err("EIO")

    def _err(self, name):
        return Rerror, ps(ERRNO_STR.get(name, name))

    # ---- 連線迴圈 ----
    def serve_conn(self, conn):
        fids = {}
        f = conn.makefile("rwb")
        while True:
            hdr = f.read(4)
            if len(hdr) < 4:
                break
            size = struct.unpack("<I", hdr)[0]
            body = f.read(size - 4)
            mtype = body[0]
            tag = struct.unpack_from("<H", body, 1)[0]
            r = Reader(body); r.i = 3
            try:
                rtype, rbody = self.dispatch(mtype, tag, r, fids)
            except Exception:
                rtype, rbody = Rerror, ps("i/o error")
            msg = p4(4 + 1 + 2 + len(rbody)) + bytes([rtype]) + p2(tag) + rbody
            f.write(msg); f.flush()


def serve(port):
    env = Env()
    np = NineP(env)
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port)); srv.listen(8)
    print(f"unipath-9p 監聽 127.0.0.1:{port}（真 9P2000）", flush=True)
    while True:
        conn, _ = srv.accept()
        threading.Thread(target=np.serve_conn, args=(conn,), daemon=True).start()


# ---- 獨立的真 9P client（自測線格式互通）----
def selftest(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    def rpc(mtype, body):
        msg = p4(7 + len(body)) + bytes([mtype]) + p2(0) + body
        s.sendall(msg)
        hdr = s.recv(4); size = struct.unpack("<I", hdr)[0]
        rest = b""
        while len(rest) < size - 4:
            rest += s.recv(size - 4 - len(rest))
        return rest[0], rest[3:]

    def rd(mtype, body):
        rt, b = rpc(mtype, body)
        if rt == Rerror:
            raise RuntimeError("Rerror: " + Reader(b).s())
        return Reader(b)

    print("Tversion…"); r = rd(Tversion, p4(65536) + ps("9P2000"))
    print("  Rversion msize=%d ver=%s" % (r.u4(), r.s()))
    print("Tattach fid=0…"); r = rd(Tattach, p4(0) + p4(NOFID) + ps("me") + ps(""))
    print("  root qid ok")

    def walk(newfid, names, base=0):
        body = p4(base) + p4(newfid) + p2(len(names)) + b"".join(ps(n) for n in names)
        r = rd(Twalk, body); nq = r.u2()
        return nq == len(names)

    # walk 到 /0/data 讀（跨真 9P 讀活 counter）
    walk(1, ["0", "data"])
    rd(Topen, p4(1) + bytes([0]))
    r = rd(Tread, p4(1) + p8(0) + p4(4096)); n = r.u4()
    print("read /0/data =", r.b[r.i:r.i + n].decode().strip())
    rd(Tclunk, p4(1))

    # walk 到 root 讀目錄項
    walk(2, [])
    rd(Topen, p4(2) + bytes([0]))
    r = rd(Tread, p4(2) + p8(0) + p4(8192)); n = r.u4()
    dirblob = r.b[r.i:r.i + n]
    names = []
    rr = Reader(dirblob)
    while rr.i < len(dirblob):
        sz = rr.u2(); end = rr.i + sz
        rr.u2(); rr.u4(); rr.i += 13; rr.u4(); rr.u4(); rr.u4(); rr.u8()
        names.append(rr.s()); rr.i = end
    print("readdir / =", names)
    rd(Tclunk, p4(2))

    # 跨真 9P 寫（write-through 改活物件）
    walk(3, ["2", "hp", "data"])
    rd(Topen, p4(3) + bytes([1]))  # OWRITE
    payload = b"555"
    rd(Twrite, p4(3) + p8(0) + p4(len(payload)) + payload)
    rd(Tclunk, p4(3))
    walk(4, ["2", "hp", "data"]); rd(Topen, p4(4) + bytes([0]))
    r = rd(Tread, p4(4) + p8(0) + p4(4096)); n = r.u4()
    print("write→read /2/hp/data =", r.b[r.i:r.i + n].decode().strip())
    s.close()
    print("SELFTEST OK：真 9P2000 線格式雙向互通")


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("serve", "selftest"):
        sys.exit(f"用法: {sys.argv[0]} serve|selftest <port>")
    (serve if sys.argv[1] == "serve" else selftest)(int(sys.argv[2]))
