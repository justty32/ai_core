// env.cpp — Env 導航與讀寫邏輯（對應 unipath_env.py）。
#include "env.hpp"
#include <thread>
#include <chrono>
#include <cstdint>

static const char* LEAVES[3] = {"data", "ctl", "status"};

static std::vector<std::string> splitPath(const std::string& p) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : p) {
        if (c == '/') { if (!cur.empty()) { out.push_back(cur); cur.clear(); } }
        else cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

Env::Env() {
    world_.k = Value::LIST;
    world_.list.push_back(Value::Int(0));                                   // /0 counter
    Value l; l.k = Value::LIST;                                             // /1 list
    for (auto s : {"alpha", "beta", "gamma"}) l.list.push_back(Value::Str(s));
    world_.list.push_back(l);
    Value d; d.k = Value::DICT;                                             // /2 dict
    d.dict.emplace_back("name", Value::Str("world"));
    d.dict.emplace_back("hp", Value::Int(100));
    world_.list.push_back(d);
}

void Env::startTicker() {
    std::thread([this] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::lock_guard<std::mutex> g(lock_);
            if (!world_.list.empty() && world_.list[0].k == Value::INT)
                world_.list[0].i += 1;
        }
    }).detach();
}

bool Env::isLeaf(const std::string& s) {
    for (auto* L : LEAVES) if (s == L) return true;
    return false;
}

Value* Env::child(Value* c, const std::string& seg) {
    if (c->k == Value::LIST) {
        try {
            size_t pos; long idx = std::stol(seg, &pos);
            if (pos == seg.size() && idx >= 0 && idx < (long)c->list.size())
                return &c->list[idx];
        } catch (...) {}
        throw UnipathError("ENOENT");
    }
    if (c->k == Value::DICT)
        for (auto& kv : c->dict) if (kv.first == seg) return &kv.second;
    throw UnipathError("ENOENT");
}

void Env::resolve(const std::string& path, Value*& parent, Value*& node, std::string& leaf) {
    auto parts = splitPath(path);
    leaf.clear();
    if (!parts.empty() && isLeaf(parts.back())) { leaf = parts.back(); parts.pop_back(); }
    parent = nullptr; node = &world_;
    for (auto& seg : parts) {
        if (!node->container()) throw UnipathError("ENOENT");
        parent = node; node = child(node, seg);
    }
}
