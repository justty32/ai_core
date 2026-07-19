#!/usr/bin/env python3
"""unipath step 2 — 真執行態環境：把一個活 Python process 的物件圖暴露成路徑樹。

與 step 1（假靜態樹）的關鍵差別：這裡的路徑樹不是寫死的資料，而是**這個正在跑的
Python process 自己的物件**的即時視窗——
  - 背景有一條 thread 持續變動 world（counter 每秒 +1），
    所以不同時刻 `cat` 同一路徑得到**不同的值**（＝執行態，非快照）；
  - 對路徑 `echo` 寫入，會**改到活物件本身**（write-through）。

物件→路徑映射（同時示範筆記「list 用數字、dict 用字串鍵，皆自然落成路徑」）：
  list/tuple → 數字子目錄 0..n-1（0-based）
  dict       → 字串鍵子目錄
  任何節點   → 目錄，內含 Plan 9 慣例 data/ctl/status
  data   讀＝該物件當前值（scalar 給值本身、容器給 repr）；寫＝改父容器該位置的活物件
  ctl    寫命令：append <literal> | set <key> <literal> | del <key>
  status 讀＝type/len/id 摘要

這就是「執行態可路徑定址」（/proc 式）的最小落地。設計脈絡見 AGENTS.md → 筆記鏈。
"""
import ast
import errno
import stat
import threading
import time
from fuse import FUSE, Operations, FuseOSError

LEAVES = ("data", "ctl", "status")
CONTAINER = (list, tuple, dict)


def build_live_world():
    """本 process 的執行態環境——會被背景 thread 持續變動。"""
    return [
        0,                              # /0  counter，背景 thread 每秒 +1
        ["alpha", "beta", "gamma"],     # /1  list → 數字子路徑 /1/0 /1/1 /1/2
        {"name": "world", "hp": 100},   # /2  dict → 字串鍵子路徑 /2/name /2/hp
    ]


class UnipathLive(Operations):
    def __init__(self):
        self.world = build_live_world()
        self.lock = threading.Lock()
        self.t0 = time.time()
        t = threading.Thread(target=self._ticker, daemon=True)
        t.start()

    def _ticker(self):
        """執行態的證據：持續改 world[0]，讓 FS 讀到的值隨時間變。"""
        while True:
            time.sleep(1)
            with self.lock:
                if self.world and isinstance(self.world[0], int):
                    self.world[0] += 1

    # ---- 路徑導航：回傳 (parent, key, value, leaf) ----
    def _child_key(self, container, seg):
        """把路徑段 seg 解成 container 裡的真實鍵。"""
        if isinstance(container, (list, tuple)):
            i = int(seg)
            if not (0 <= i < len(container)):
                raise FuseOSError(errno.ENOENT)
            return i
        if isinstance(container, dict):
            for k in container:
                if str(k) == seg:
                    return k
        raise FuseOSError(errno.ENOENT)

    def _resolve(self, path):
        parts = [p for p in path.split("/") if p]
        leaf = None
        if parts and parts[-1] in LEAVES:
            leaf = parts.pop()
        parent, key, value = None, None, self.world
        for seg in parts:
            if not isinstance(value, CONTAINER):
                raise FuseOSError(errno.ENOENT)   # scalar 底下無子節點
            k = self._child_key(value, seg)
            parent, key, value = value, k, value[k]
        return parent, key, value, leaf

    def _children(self, value):
        if isinstance(value, (list, tuple)):
            return [str(i) for i in range(len(value))]
        if isinstance(value, dict):
            return [str(k) for k in value]
        return []

    def _data_bytes(self, value):
        if isinstance(value, CONTAINER):
            return (repr(value) + "\n").encode()
        return (str(value) + "\n").encode()

    def _status_bytes(self, value):
        n = len(value) if isinstance(value, CONTAINER) else "-"
        return (f"type={type(value).__name__} len={n} id={id(value)}\n").encode()

    def _leaf_bytes(self, value, leaf):
        if leaf == "data":
            return self._data_bytes(value)
        if leaf == "status":
            return self._status_bytes(value)
        if leaf == "ctl":
            return b"# append <literal> | set <key> <literal> | del <key>\n"
        raise FuseOSError(errno.ENOENT)

    # ---- FUSE 操作 ----
    def getattr(self, path, fh=None):
        with self.lock:
            _, _, value, leaf = self._resolve(path)
            now = self.t0
            if leaf is None:
                return dict(st_mode=stat.S_IFDIR | 0o755, st_nlink=2,
                            st_ctime=now, st_mtime=now, st_atime=now)
            mode = 0o444 if leaf == "status" else 0o644
            return dict(st_mode=stat.S_IFREG | mode, st_nlink=1,
                        st_size=len(self._leaf_bytes(value, leaf)),
                        st_ctime=now, st_mtime=now, st_atime=now)

    def readdir(self, path, fh):
        with self.lock:
            _, _, value, leaf = self._resolve(path)
            if leaf is not None:
                raise FuseOSError(errno.ENOTDIR)
            return [".", ".."] + list(LEAVES) + self._children(value)

    def open(self, path, flags):
        with self.lock:
            self._resolve(path)
        return 0

    def read(self, path, size, offset, fh):
        with self.lock:
            _, _, value, leaf = self._resolve(path)
            if leaf is None:
                raise FuseOSError(errno.EISDIR)
            return self._leaf_bytes(value, leaf)[offset:offset + size]

    def truncate(self, path, length, fh=None):
        return 0  # scalar 寫入用整段取代，無需真截斷

    @staticmethod
    def _parse(text):
        text = text.strip()
        try:
            return ast.literal_eval(text)   # 42 / 3.14 / 'x' / [1,2] / {...}
        except (ValueError, SyntaxError):
            return text                     # 純文字

    def write(self, path, data, offset, fh):
        with self.lock:
            parent, key, value, leaf = self._resolve(path)
            if leaf == "data":
                if parent is None:
                    raise FuseOSError(errno.EROFS)      # 不可整包取代 root
                if isinstance(parent, tuple):
                    raise FuseOSError(errno.EROFS)      # tuple 不可變
                parent[key] = self._parse(data.decode(errors="replace"))
                return len(data)
            if leaf == "ctl":
                for line in data.decode(errors="replace").splitlines():
                    if line.strip():
                        self._run_ctl(value, line.strip())
                return len(data)
            raise FuseOSError(errno.EROFS)              # status 唯讀

    def _run_ctl(self, value, line):
        cmd, _, rest = line.partition(" ")
        if cmd == "append" and isinstance(value, list):
            value.append(self._parse(rest))
        elif cmd == "set":
            k, _, v = rest.partition(" ")
            if isinstance(value, list):
                value[int(k)] = self._parse(v)
            elif isinstance(value, dict):
                value[k] = self._parse(v)
        elif cmd == "del":
            if isinstance(value, list):
                del value[int(rest)]
            elif isinstance(value, dict):
                value.pop(rest, None)
        # 未知命令：忽略（試驗田心態）


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        sys.exit(f"用法: {sys.argv[0]} <mountpoint>")
    FUSE(UnipathLive(), sys.argv[1], foreground=True, nothreads=True)
