// ninep.cpp — NineP 的 qid / stat 編碼與小工具（對應 unipath_9p.py）。
#include "ninep.hpp"

static std::string errStr(const std::string& name) {
    if (name == "ENOENT") return "no such file";
    if (name == "ENOTDIR") return "not a directory";
    if (name == "EISDIR") return "is a directory";
    if (name == "EROFS") return "read-only";
    if (name == "EIO") return "i/o error";
    return name;
}

bool NineP::isdir(const std::string& path) {
    return env_.stat(path).isdir;
}

std::string NineP::qid(const std::string& path) {
    uint64_t pnum;
    {
        std::lock_guard<std::mutex> g(qidLock_);
        auto it = qidPaths_.find(path);
        if (it == qidPaths_.end()) { pnum = qidNext_++; qidPaths_[path] = pnum; }
        else pnum = it->second;
    }
    std::string o;
    p1(o, isdir(path) ? QTDIR : 0);
    p4(o, 0);          // version 恆 0（誠實簡化，同 Python）
    p8(o, pnum);       // path＝穩定 inode 號
    return o;
}

std::string NineP::childPath(const std::string& path, const std::string& w) {
    std::string base = (path == "/") ? "" : path;
    return base + "/" + w;
}

std::string NineP::leafName(const std::string& path) {
    std::string last, cur;
    for (char c : path) {
        if (c == '/') { if (!cur.empty()) { last = cur; cur.clear(); } }
        else cur.push_back(c);
    }
    if (!cur.empty()) last = cur;
    return last.empty() ? "unipath" : last;
}

std::string NineP::statBytes(const std::string& path) {
    StatInfo st = env_.stat(path);
    uint32_t mode = st.isdir ? (DMDIR | 0755) : (st.ro ? 0444 : 0644);
    uint64_t length = st.isdir ? 0 : st.size;
    std::string inner;
    p2(inner, 0);                       // type[2]
    p4(inner, 0);                       // dev[4]
    inner += qid(path);                 // qid[13]
    p4(inner, mode);
    p4(inner, 0); p4(inner, 0);         // atime, mtime
    p8(inner, length);
    ps(inner, leafName(path));
    ps(inner, "unipath"); ps(inner, "unipath"); ps(inner, "unipath");  // uid gid muid
    std::string o;
    p2(o, (uint16_t)inner.size());      // 內層 size[2]
    o += inner;
    return o;
}

std::pair<uint8_t, std::string> NineP::err(const std::string& name) {
    std::string o;
    ps(o, errStr(name));
    return {(uint8_t)Rerror, o};
}
