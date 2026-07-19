#!/usr/bin/env python3
"""unipath step 1 — 假靜態樹，證明 FUSE mount 管線通。

把一棵寫死的 0-based 數字路徑樹掛成檔案系統。每個「元素」是一個目錄，
內含 Plan 9 慣例的三個約定檔：
  data    可讀可寫——元素的內容
  ctl     只寫——控制命令（取代 ioctl）；每次 write 當一批命令行
  status  只讀——元素狀態摘要（衍生自 data/children/最後命令）
數字子目錄 0,1,2… 為子元素（0-based）。

這一刀只為驗證 `ls`/`cat`/`echo >` 能 walk、讀、寫；
真執行態環境的暴露留 step 2。設計脈絡見 AGENTS.md → 筆記鏈。
"""
import errno
import stat
import time
from fuse import FUSE, Operations, FuseOSError

LEAVES = ("data", "ctl", "status")  # 每個元素目錄的約定檔


class Element:
    """樹上一個節點：有內容(data)、命令日誌(log)、數字鍵的子元素。"""

    def __init__(self, data=b""):
        self.data = data
        self.log = []           # ctl 收到的命令歷史
        self.children = {}      # "0" -> Element（0-based 數字鍵）

    def add_child(self, data=b""):
        idx = str(len(self.children))   # 下一個 0-based 索引
        self.children[idx] = Element(data)
        return idx

    def status_bytes(self):
        kids = ",".join(sorted(self.children, key=int)) or "-"
        last = self.log[-1] if self.log else "-"
        return (f"data={len(self.data)}B children=[{kids}] "
                f"ctl_count={len(self.log)} last_ctl={last!r}\n").encode()


def build_fake_tree():
    """寫死的示範樹——只為證明管線通。"""
    root = Element(b"unipath root\n")
    e0 = root.children["0"] = Element(b"hello from element 0\n")
    e0.children["0"] = Element(b"nested element 0/0\n")
    root.children["1"] = Element(b"element 1\n")
    root.children["2"] = Element(b"element 2\n")
    return root


class UnipathFS(Operations):
    def __init__(self):
        self.root = build_fake_tree()
        self.t0 = time.time()

    # ---- 路徑解析：path -> (element, leaf)。leaf ∈ {None=目錄, 'data'/'ctl'/'status'} ----
    def _resolve(self, path):
        parts = [p for p in path.split("/") if p]
        leaf = None
        if parts and parts[-1] in LEAVES:
            leaf = parts.pop()
        node = self.root
        for p in parts:
            if p not in node.children:
                raise FuseOSError(errno.ENOENT)
            node = node.children[p]
        return node, leaf

    def _leaf_bytes(self, node, leaf):
        if leaf == "data":
            return node.data
        if leaf == "status":
            return node.status_bytes()
        if leaf == "ctl":
            return ("# 寫入命令：set <text> | mkelem | rmelem <n>\n"
                    "# 歷史：\n" + "".join(c + "\n" for c in node.log)).encode()
        raise FuseOSError(errno.ENOENT)

    # ---- FUSE 操作 ----
    def getattr(self, path, fh=None):
        node, leaf = self._resolve(path)
        now = self.t0
        if leaf is None:  # 目錄
            return dict(st_mode=stat.S_IFDIR | 0o755, st_nlink=2,
                        st_ctime=now, st_mtime=now, st_atime=now)
        mode = 0o444 if leaf == "status" else 0o644
        return dict(st_mode=stat.S_IFREG | mode, st_nlink=1,
                    st_size=len(self._leaf_bytes(node, leaf)),
                    st_ctime=now, st_mtime=now, st_atime=now)

    def readdir(self, path, fh):
        node, leaf = self._resolve(path)
        if leaf is not None:
            raise FuseOSError(errno.ENOTDIR)
        return [".", ".."] + list(LEAVES) + sorted(node.children, key=int)

    def open(self, path, flags):
        self._resolve(path)  # 存在性檢查
        return 0

    def read(self, path, size, offset, fh):
        node, leaf = self._resolve(path)
        if leaf is None:
            raise FuseOSError(errno.EISDIR)
        return self._leaf_bytes(node, leaf)[offset:offset + size]

    def truncate(self, path, length, fh=None):
        node, leaf = self._resolve(path)
        if leaf == "data":
            node.data = node.data[:length]
        return 0

    def write(self, path, data, offset, fh):
        node, leaf = self._resolve(path)
        if leaf == "data":
            node.data = node.data[:offset] + data + node.data[offset + len(data):]
            return len(data)
        if leaf == "ctl":
            for line in data.decode(errors="replace").splitlines():
                line = line.strip()
                if line:
                    self._run_ctl(node, line)
            return len(data)
        raise FuseOSError(errno.EROFS)  # status 唯讀

    def _run_ctl(self, node, line):
        node.log.append(line)
        cmd, _, arg = line.partition(" ")
        if cmd == "set":
            node.data = (arg + "\n").encode()
        elif cmd == "mkelem":
            node.add_child(b"new element\n")
        elif cmd == "rmelem":
            node.children.pop(arg.strip(), None)
        # 未知命令：只記日誌，不報錯（試驗田心態）


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        sys.exit(f"用法: {sys.argv[0]} <mountpoint>")
    FUSE(UnipathFS(), sys.argv[1], foreground=True, nothreads=True)
