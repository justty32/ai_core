"""unipath 執行態環境：活物件圖，可路徑導航與讀寫、可背景 tick。
對外 API（stat/readdir/read/write）被 up_fuse（FUSE）與 up_ninep（9P）共用。"""
import threading
import time

from up_world import (LEAVES, CONTAINER, UnipathError, build_live_world,
                      children, leaf_bytes, parse)

class Env:
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

    def stat(self, path):
        with self.lock:
            _, _, value, leaf = self._resolve(path)
            if leaf is None:
                return {"kind": "dir", "size": 0, "ro": False}
            return {"kind": "file", "size": len(leaf_bytes(value, leaf)),
                    "ro": leaf == "status"}

    def readdir(self, path):
        with self.lock:
            _, _, value, leaf = self._resolve(path)
            if leaf is not None:
                raise UnipathError("ENOTDIR")
            return list(LEAVES) + children(value)

    def read(self, path):
        with self.lock:
            _, _, value, leaf = self._resolve(path)
            if leaf is None:
                raise UnipathError("EISDIR")
            return leaf_bytes(value, leaf)

    def write(self, path, data):
        with self.lock:
            parent, key, value, leaf = self._resolve(path)
            if leaf == "data":
                if parent is None or isinstance(parent, tuple):
                    raise UnipathError("EROFS")
                parent[key] = parse(data.decode(errors="replace"))
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
            value.append(parse(rest))
        elif cmd == "set":
            k, _, v = rest.partition(" ")
            if isinstance(value, list):
                value[int(k)] = parse(v)
            elif isinstance(value, dict):
                value[k] = parse(v)
        elif cmd == "del":
            if isinstance(value, list):
                del value[int(rest)]
            elif isinstance(value, dict):
                value.pop(rest, None)
