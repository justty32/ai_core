// env.hpp — 活執行態環境（純邏輯，無 socket）。對應 unipath_env.py 的 Env。
// 樹：/0=counter(int,背景每秒+1)、/1=list、/2=dict；每節點含 data/ctl/status 三葉子。
#pragma once
#include "value.hpp"
#include <mutex>
#include <string>
#include <vector>

struct UnipathError {
    std::string name;  // errno 符號名，如 "ENOENT"
    explicit UnipathError(std::string n) : name(std::move(n)) {}
};

struct StatInfo { bool isdir; size_t size; bool ro; };

class Env {
public:
    Env();
    void startTicker();               // 背景 thread：world[0] 每秒 +1
    StatInfo stat(const std::string& path);
    std::vector<std::string> readdir(const std::string& path);
    std::string read(const std::string& path);       // 回葉子位元組
    size_t write(const std::string& path, const std::string& data);

private:
    Value world_;
    std::mutex lock_;

    // 導航：拆路徑、剝出葉子名，回傳目標節點與其父。leaf 為空＝目錄。
    void resolve(const std::string& path, Value*& parent, Value*& node, std::string& leaf);
    Value* child(Value* c, const std::string& seg);  // 走一段，找不到丟 ENOENT
    std::vector<std::string> children(const Value* v);
    std::string leafBytes(const Value* v, const std::string& leaf);
    static bool isLeaf(const std::string& s);
    static Value parseLiteral(const std::string& text);
};
