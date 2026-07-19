"""unipath step 1 — 假靜態樹的 env（供 up_fuse 掛載，證 mount 管線）。
Element：data 內容＋ctl 日誌＋數字鍵子元素；ctl：set/mkelem/rmelem。
提供與 up_env.Env 相同的 stat/readdir/read/write 介面，故共用 up_fuse。"""
from up_world import LEAVES, UnipathError


class Element:
    def __init__(self, data=b""):
        self.data = data
        self.log = []
        self.children = {}

    def add_child(self, data=b""):
        idx = str(len(self.children))
        self.children[idx] = Element(data)
        return idx

    def status(self):
        kids = ",".join(sorted(self.children, key=int)) or "-"
        last = self.log[-1] if self.log else "-"
        return (f"data={len(self.data)}B children=[{kids}] "
                f"ctl_count={len(self.log)} last_ctl={last!r}\n").encode()


def build_fake_tree():
    root = Element(b"unipath root\n")
    e0 = root.children["0"] = Element(b"hello from element 0\n")
    e0.children["0"] = Element(b"nested element 0/0\n")
    root.children["1"] = Element(b"element 1\n")
    root.children["2"] = Element(b"element 2\n")
    return root


class StaticEnv:
    def __init__(self):
        self.root = build_fake_tree()

    def _resolve(self, path):
        parts = [p for p in path.split("/") if p]
        leaf = None
        if parts and parts[-1] in LEAVES:
            leaf = parts.pop()
        node = self.root
        for p in parts:
            if p not in node.children:
                raise UnipathError("ENOENT")
            node = node.children[p]
        return node, leaf

    def _leaf(self, node, leaf):
        if leaf == "data":
            return node.data
        if leaf == "status":
            return node.status()
        if leaf == "ctl":
            return (b"# set <text> | mkelem | rmelem <n>\n"
                    + "".join(c + "\n" for c in node.log).encode())
        raise UnipathError("ENOENT")

    def stat(self, path):
        node, leaf = self._resolve(path)
        if leaf is None:
            return {"kind": "dir", "size": 0, "ro": False}
        return {"kind": "file", "size": len(self._leaf(node, leaf)), "ro": leaf == "status"}

    def readdir(self, path):
        node, leaf = self._resolve(path)
        if leaf is not None:
            raise UnipathError("ENOTDIR")
        return list(LEAVES) + sorted(node.children, key=int)

    def read(self, path):
        node, leaf = self._resolve(path)
        if leaf is None:
            raise UnipathError("EISDIR")
        return self._leaf(node, leaf)

    def write(self, path, data):
        node, leaf = self._resolve(path)
        if leaf == "data":
            node.data = data
            return len(data)
        if leaf == "ctl":
            for line in data.decode(errors="replace").splitlines():
                if line.strip():
                    self._ctl(node, line.strip())
            return len(data)
        raise UnipathError("EROFS")

    def _ctl(self, node, line):
        node.log.append(line)
        cmd, _, arg = line.partition(" ")
        if cmd == "set":
            node.data = (arg + "\n").encode()
        elif cmd == "mkelem":
            node.add_child(b"new element\n")
        elif cmd == "rmelem":
            node.children.pop(arg.strip(), None)
