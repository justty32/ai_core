"""共用 FUSE 轉接器：把任何具 stat/readdir/read/write 的 env 掛成檔案系統。

step 1（up_static）與 step 2（up_env）共用這一層——差別只在背後的 env。
"""
import errno
import stat as statmod

from fuse import Operations, FuseOSError

from up_world import UnipathError


def _err(e):
    return FuseOSError(getattr(errno, e.name, errno.EIO))


class FuseEnv(Operations):
    def __init__(self, env):
        self.env = env

    def getattr(self, path, fh=None):
        try:
            st = self.env.stat(path)
        except UnipathError as e:
            raise _err(e)
        if st["kind"] == "dir":
            return dict(st_mode=statmod.S_IFDIR | 0o755, st_nlink=2)
        mode = 0o444 if st["ro"] else 0o644
        return dict(st_mode=statmod.S_IFREG | mode, st_nlink=1, st_size=st["size"])

    def readdir(self, path, fh):
        try:
            return [".", ".."] + self.env.readdir(path)
        except UnipathError as e:
            raise _err(e)

    def open(self, path, flags):
        try:
            self.env.stat(path)
        except UnipathError as e:
            raise _err(e)
        return 0

    def read(self, path, size, offset, fh):
        try:
            return self.env.read(path)[offset:offset + size]
        except UnipathError as e:
            raise _err(e)

    def truncate(self, path, length, fh=None):
        return 0

    def write(self, path, data, offset, fh):
        try:
            return self.env.write(path, data)
        except UnipathError as e:
            raise _err(e)
