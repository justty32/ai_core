// ninep.hpp — 9P2000 訊息分派（對應 unipath_9p.py 的 NineP）。
#pragma once
#include "env.hpp"
#include "codec.hpp"
#include <map>
#include <mutex>
#include <string>
#include <vector>

// 訊息型別（偶數 T、奇數 R = T+1）
enum {
    Tversion = 100, Rversion = 101, Tattach = 104, Rattach = 105, Rerror = 107,
    Tflush = 108, Rflush = 109, Twalk = 110, Rwalk = 111, Topen = 112, Ropen = 113,
    Tcreate = 114, Tread = 116, Rread = 117, Twrite = 118, Rwrite = 119,
    Tclunk = 120, Rclunk = 121, Tremove = 122, Tstat = 124, Rstat = 125, Twstat = 126
};
static const uint8_t QTDIR = 0x80;
static const uint32_t DMDIR = 0x80000000u;

struct Fid {
    std::string path;
    bool open = false;
    bool isdir = false;
    std::vector<std::string> dirents;   // 目錄的預渲染 stat blob 列表
};

class NineP {
public:
    explicit NineP(Env& env) : env_(env) {}
    // 回傳 (rtype, rbody)。fids 為本連線私有的 fid 表。
    std::pair<uint8_t, std::string> dispatch(uint8_t mtype, Reader& r,
                                             std::map<uint32_t, Fid>& fids);

private:
    Env& env_;
    uint32_t msize_ = 65536;
    std::map<std::string, uint64_t> qidPaths_;   // 路徑→穩定 qid.path（跨連線共享）
    uint64_t qidNext_ = 0;
    std::mutex qidLock_;

    bool isdir(const std::string& path);
    std::string qid(const std::string& path);
    std::string statBytes(const std::string& path);
    std::string childPath(const std::string& path, const std::string& w);
    static std::string leafName(const std::string& path);
    std::pair<uint8_t, std::string> err(const std::string& name);
};
