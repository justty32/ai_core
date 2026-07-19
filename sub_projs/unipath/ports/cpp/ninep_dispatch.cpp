// ninep_dispatch.cpp — 逐訊息分派（對應 unipath_9p.py 的 dispatch）。
#include "ninep.hpp"
#include <algorithm>

std::pair<uint8_t, std::string> NineP::dispatch(uint8_t mtype, Reader& r,
                                                std::map<uint32_t, Fid>& fids) {
    if (mtype == Tversion) {
        uint32_t ms = r.u4(); std::string ver = r.s();
        msize_ = std::min(msize_, ms);
        std::string v = ver.rfind("9P2000", 0) == 0 ? "9P2000" : "unknown";
        std::string o; p4(o, msize_); ps(o, v); return {Rversion, o};
    }
    if (mtype == Tattach) {
        uint32_t fid = r.u4(); r.u4(); r.s(); r.s();
        fids[fid] = Fid{"/", false, false, {}};
        return {Rattach, qid("/")};
    }
    if (mtype == Twalk) {
        uint32_t fid = r.u4(), newfid = r.u4(); uint16_t n = r.u2();
        std::vector<std::string> names;
        for (int k = 0; k < n; ++k) names.push_back(r.s());
        auto it = fids.find(fid);
        if (it == fids.end()) return err("ENOENT");
        std::string path = it->second.path, qids;
        int got = 0;
        for (auto& w : names) {
            std::string cand = (w == "..") ? "/" : childPath(path, w);
            try { env_.stat(cand); } catch (UnipathError&) { break; }
            path = cand; qids += qid(path); ++got;
        }
        if (got != n && got == 0) return err("ENOENT");
        if (got == n) fids[newfid] = Fid{path, false, false, {}};
        std::string o; p2(o, (uint16_t)got); o += qids; return {Rwalk, o};
    }
    if (mtype == Topen) {
        uint32_t fid = r.u4(); r.u1();
        auto it = fids.find(fid);
        if (it == fids.end()) return err("ENOENT");
        Fid& st = it->second; st.open = true;
        if (isdir(st.path)) {
            st.isdir = true;
            for (auto& c : env_.readdir(st.path))
                st.dirents.push_back(statBytes(childPath(st.path, c)));
        }
        std::string o; o += qid(st.path); p4(o, 0); return {Ropen, o};
    }
    if (mtype == Tread) {
        uint32_t fid = r.u4(); uint64_t off = r.u8(); uint32_t count = r.u4();
        auto it = fids.find(fid);
        if (it == fids.end()) return err("ENOENT");
        Fid& st = it->second;
        std::string out;
        if (st.isdir) {
            uint64_t pos = 0;
            for (auto& e : st.dirents) {
                if (pos < off) { pos += e.size(); continue; }
                if (out.size() + e.size() > count) break;
                out += e; pos += e.size();
            }
        } else {
            std::string data;
            try { data = env_.read(st.path); } catch (UnipathError& e) { return err(e.name); }
            if (off < data.size()) out = data.substr(off, count);
        }
        std::string o; p4(o, (uint32_t)out.size()); o += out; return {Rread, o};
    }
    if (mtype == Twrite) {
        uint32_t fid = r.u4(); r.u8(); uint32_t count = r.u4();
        std::string data((const char*)r.b + r.i, count);
        auto it = fids.find(fid);
        if (it == fids.end()) return err("ENOENT");
        size_t nn;
        try { nn = env_.write(it->second.path, data); }
        catch (UnipathError& e) { return err(e.name); }
        std::string o; p4(o, (uint32_t)nn); return {Rwrite, o};
    }
    if (mtype == Tstat) {
        uint32_t fid = r.u4();
        auto it = fids.find(fid);
        if (it == fids.end()) return err("ENOENT");
        std::string sb = statBytes(it->second.path), o;
        p2(o, (uint16_t)sb.size()); o += sb; return {Rstat, o};
    }
    if (mtype == Tclunk) { fids.erase(r.u4()); return {Rclunk, ""}; }
    if (mtype == Tflush) { r.u2(); return {Rflush, ""}; }
    if (mtype == Tcreate || mtype == Tremove || mtype == Twstat) return err("EROFS");
    return err("EIO");
}
