#!/usr/bin/env python3
"""unipath — 執行態環境的純邏輯（無 FUSE、無 socket 依賴）。

把一個活 Python 物件圖當「環境」，提供路徑導航與讀寫。
step 2 的邏輯抽到這裡，好讓 step 3 的發布端（unipath_pub.py）跨 process 共用。

映射（同筆記「list 用數字、dict 用字串鍵，皆自然落成路徑」）：
  list/tuple → 數字子路徑 0..n-1（0-based）；dict → 字串鍵子路徑
  任何節點皆為目錄，含 Plan 9 慣例 data/ctl/status
"""
import ast
import threading
import time

LEAVES = ("data", "ctl", "status")
CONTAINER = (list, tuple, dict)


class UnipathError(Exception):
    """帶 errno 名（如 'ENOENT'）的錯誤，跨邊界時轉成對應 errno。"""
    def __init__(self, name):
        super().__init__(name)
        self.name = name


def build_live_world():
    return [
        0,                              # /0  counter，背景 thread 每秒 +1
        ["alpha", "beta", "gamma"],     # /1  list
        {"name": "world", "hp": 100},   # /2  dict
    ]


class Env:
    """一個活物件圖的執行態環境，可被路徑導航／讀寫，且會被背景 tick 變動。"""

    def __init__(self, world=None, tick=True):
        self.world = build_live_world() if world is None else world
        self.lock = threading.Lock()
        if tick:
            threading.Thread(target=self._ticker, daemon=True).start()

    def _ticker(self):
        while True:
            time.sleep(1)
            with self.lock:
                if self.world and isinstance(self.world[0], int):
                    self.world[0] += 1

    # ---- 導航 ----
    def _child_key(self, container, seg):
        if isinstance(container, (list, tuple)):
            i = int(seg)
            if not (0 <= i < len(container)):
                raise UnipathError("ENOENT")
            return i
        if isinstance(container, dict):
            for k in container:
                if str(k) == seg:
                    return k
        raise UnipathError("ENOENT")

    def _resolve(self, path):
        parts = [p for p in path.split("/") if p]
        leaf = None
        if parts and parts[-1] in LEAVES:
            leaf = parts.pop()
        parent, key, value = None, None, self.world
        for seg in parts:
            if not isinstance(value, CONTAINER):
                raise UnipathError("ENOENT")
            k = self._child_key(value, seg)
            parent, key, value = value, k, value[k]
        return parent, key, value, leaf

    def _children(self, value):
        if isinstance(value, (list, tuple)):
            return [str(i) for i in range(len(value))]
        if isinstance(value, dict):
            return [str(k) for k in value]
        return []

    def _leaf_bytes(self, value, leaf):
        if leaf == "data":
            body = repr(value) if isinstance(value, CONTAINER) else str(value)
            return (body + "\n").encode()
        if leaf == "status":
            n = len(value) if isinstance(value, CONTAINER) else "-"
            return f"type={type(value).__name__} len={n} id={id(value)}\n".encode()
        if leaf == "ctl":
            return b"# append <literal> | set <key> <literal> | del <key>\n"
        raise UnipathError("ENOENT")

    @staticmethod
    def _parse(text):
        text = text.strip()
        try:
            return ast.literal_eval(text)
        except (ValueError, SyntaxError):
            return text

    # ---- 對外 API（發布端與自掛端共用）----
    def stat(self, path):
        with self.lock:
            _, _, value, leaf = self._resolve(path)
            if leaf is None:
                return {"kind": "dir", "size": 0, "ro": False}
            return {"kind": "file", "size": len(self._leaf_bytes(value, leaf)),
                    "ro": leaf == "status"}

    def readdir(self, path):
        with self.lock:
            _, _, value, leaf = self._resolve(path)
            if leaf is not None:
                raise UnipathError("ENOTDIR")
            return list(LEAVES) + self._children(value)

    def read(self, path):
        with self.lock:
            _, _, value, leaf = self._resolve(path)
            if leaf is None:
                raise UnipathError("EISDIR")
            return self._leaf_bytes(value, leaf)

    def write(self, path, data):
        with self.lock:
            parent, key, value, leaf = self._resolve(path)
            if leaf == "data":
                if parent is None or isinstance(parent, tuple):
                    raise UnipathError("EROFS")
                parent[key] = self._parse(data.decode(errors="replace"))
                return len(data)
            if leaf == "ctl":
                for line in data.decode(errors="replace").splitlines():
                    if line.strip():
                        self._run_ctl(value, line.strip())
                return len(data)
            raise UnipathError("EROFS")

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
