"""獨立的真 9P client：自測與 unipath 9P server 的線格式互通（跨語言驗收也用它）。"""
import socket
import struct

from up_ninep_codec import (Tversion, Tattach, Twalk, Topen, Tread, Twrite,
                            Tclunk, Rerror, NOFID, p2, p4, p8, ps, Reader)


def selftest(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    def rpc(mtype, body):
        s.sendall(p4(7 + len(body)) + bytes([mtype]) + p2(0) + body)
        size = struct.unpack("<I", s.recv(4))[0]
        rest = b""
        while len(rest) < size - 4:
            rest += s.recv(size - 4 - len(rest))
        return rest[0], rest[3:]

    def rd(mtype, body):
        rt, b = rpc(mtype, body)
        if rt == Rerror:
            raise RuntimeError("Rerror: " + Reader(b).s())
        return Reader(b)

    def walk(newfid, names, base=0):
        body = p4(base) + p4(newfid) + p2(len(names)) + b"".join(ps(n) for n in names)
        return rd(Twalk, body).u2() == len(names)

    print("Tversion…"); r = rd(Tversion, p4(65536) + ps("9P2000"))
    print("  Rversion msize=%d ver=%s" % (r.u4(), r.s()))
    rd(Tattach, p4(0) + p4(NOFID) + ps("me") + ps("")); print("  root qid ok")

    walk(1, ["0", "data"]); rd(Topen, p4(1) + bytes([0]))
    r = rd(Tread, p4(1) + p8(0) + p4(4096)); n = r.u4()
    print("read /0/data =", r.b[r.i:r.i + n].decode().strip()); rd(Tclunk, p4(1))

    walk(2, []); rd(Topen, p4(2) + bytes([0]))
    r = rd(Tread, p4(2) + p8(0) + p4(8192)); n = r.u4()
    blob = r.b[r.i:r.i + n]; names = []; rr = Reader(blob)
    while rr.i < len(blob):
        sz = rr.u2(); end = rr.i + sz
        rr.u2(); rr.u4(); rr.i += 13; rr.u4(); rr.u4(); rr.u4(); rr.u8()
        names.append(rr.s()); rr.i = end
    print("readdir / =", names); rd(Tclunk, p4(2))

    walk(3, ["2", "hp", "data"]); rd(Topen, p4(3) + bytes([1]))
    rd(Twrite, p4(3) + p8(0) + p4(3) + b"555"); rd(Tclunk, p4(3))
    walk(4, ["2", "hp", "data"]); rd(Topen, p4(4) + bytes([0]))
    r = rd(Tread, p4(4) + p8(0) + p4(4096)); n = r.u4()
    print("write→read /2/hp/data =", r.b[r.i:r.i + n].decode().strip())
    s.close(); print("SELFTEST OK：真 9P2000 線格式雙向互通")
