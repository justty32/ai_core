"""9P2000 線格式的編解碼工具與常數（無狀態）。整數 little-endian；字串 size[2]+utf8。"""
import struct

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
