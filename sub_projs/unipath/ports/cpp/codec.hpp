// codec.hpp — 9P2000 線格式的整數/字串編解碼。
// 整數一律 little-endian；字串＝size[2]+utf8（對應 unipath_9p.py 的 p2/p4/p8/ps/Reader）。
#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>

inline void p1(std::string& o, uint8_t v) { o.push_back((char)v); }
inline void p2(std::string& o, uint16_t v) {
    o.push_back((char)(v & 0xff)); o.push_back((char)((v >> 8) & 0xff));
}
inline void p4(std::string& o, uint32_t v) {
    for (int b = 0; b < 4; ++b) o.push_back((char)((v >> (8 * b)) & 0xff));
}
inline void p8(std::string& o, uint64_t v) {
    for (int b = 0; b < 8; ++b) o.push_back((char)((v >> (8 * b)) & 0xff));
}
inline void ps(std::string& o, const std::string& s) {
    p2(o, (uint16_t)s.size()); o += s;
}

struct Reader {
    const uint8_t* b;
    size_t n, i = 0;
    Reader(const uint8_t* buf, size_t len) : b(buf), n(len) {}
    uint8_t u1() { return b[i++]; }
    uint16_t u2() { uint16_t v = b[i] | (b[i + 1] << 8); i += 2; return v; }
    uint32_t u4() {
        uint32_t v = 0;
        for (int k = 0; k < 4; ++k) v |= (uint32_t)b[i + k] << (8 * k);
        i += 4; return v;
    }
    uint64_t u8() {
        uint64_t v = 0;
        for (int k = 0; k < 8; ++k) v |= (uint64_t)b[i + k] << (8 * k);
        i += 8; return v;
    }
    std::string s() {
        uint16_t len = u2();
        std::string r((const char*)b + i, len); i += len; return r;
    }
};
