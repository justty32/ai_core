"""9P2000 訊息分派：dispatch(np, mtype, tag, r, fids) → (rtype, rbody)。

np 提供編碼（_qid/_stat_bytes/_child_path/_isdir/_err）與 env；本檔只管解請求、組回應。
"""
from up_world import UnipathError
from up_ninep_codec import (Tversion, Rversion, Tattach, Rattach, Twalk, Rwalk,
                            Topen, Ropen, Tread, Rread, Twrite, Rwrite,
                            Tclunk, Rclunk, Tstat, Rstat, Tflush, Rflush,
                            Tcreate, Tremove, Twstat, p2, p4, ps)


def dispatch(np, mtype, tag, r, fids):
    if mtype == Tversion:
        ms = r.u4(); ver = r.s()
        np.msize = min(np.msize, ms)
        v = "9P2000" if ver.startswith("9P2000") else "unknown"
        return Rversion, p4(np.msize) + ps(v)
    if mtype == Tattach:
        fid = r.u4(); r.u4(); r.s(); r.s()   # fid afid uname aname
        fids[fid] = {"path": "/", "open": False}
        return Rattach, np._qid("/")
    if mtype == Twalk:
        fid = r.u4(); newfid = r.u4(); n = r.u2()
        names = [r.s() for _ in range(n)]
        if fid not in fids:
            return np._err("ENOENT")
        path = fids[fid]["path"]; qids = []
        for w in names:
            cand = "/" if w == ".." else np._child_path(path, w)
            try:
                np.env.stat(cand)
            except UnipathError:
                break
            path = cand; qids.append(np._qid(path))
        if len(qids) != n and not qids:
            return np._err("ENOENT")
        if len(qids) == n:
            fids[newfid] = {"path": path, "open": False}
        return Rwalk, p2(len(qids)) + b"".join(qids)
    if mtype == Topen:
        fid = r.u4(); r.u1(); st = fids.get(fid)
        if not st:
            return np._err("ENOENT")
        st["open"] = True
        if np._isdir(st["path"]):
            st["dir"] = [np._stat_bytes(np._child_path(st["path"], c))
                         for c in np.env.readdir(st["path"])]
        return Ropen, np._qid(st["path"]) + p4(0)
    if mtype == Tread:
        fid = r.u4(); off = r.u8(); count = r.u4(); st = fids.get(fid)
        if not st:
            return np._err("ENOENT")
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
            data = np.env.read(st["path"])
        except UnipathError as e:
            return np._err(e.name)
        chunk = data[off:off + count]
        return Rread, p4(len(chunk)) + chunk
    if mtype == Twrite:
        fid = r.u4(); r.u8(); count = r.u4(); data = r.b[r.i:r.i + count]; st = fids.get(fid)
        if not st:
            return np._err("ENOENT")
        try:
            n = np.env.write(st["path"], data)
        except UnipathError as e:
            return np._err(e.name)
        return Rwrite, p4(n)
    if mtype == Tstat:
        fid = r.u4(); st = fids.get(fid)
        if not st:
            return np._err("ENOENT")
        sb = np._stat_bytes(st["path"])
        return Rstat, p2(len(sb)) + sb
    if mtype == Tclunk:
        fid = r.u4(); fids.pop(fid, None)
        return Rclunk, b""
    if mtype == Tflush:
        r.u2(); return Rflush, b""
    if mtype in (Tcreate, Tremove, Twstat):
        return np._err("EROFS")
    return np._err("EIO")
