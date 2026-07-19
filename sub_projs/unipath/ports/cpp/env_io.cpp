// env_io.cpp — Env 的葉子渲染、對外 stat/readdir/read/write（對應 unipath_env.py）。
#include "env.hpp"
#include <cstdint>

std::vector<std::string> Env::children(const Value* v) {
    std::vector<std::string> out;
    if (v->k == Value::LIST)
        for (size_t i = 0; i < v->list.size(); ++i) out.push_back(std::to_string(i));
    else if (v->k == Value::DICT)
        for (auto& kv : v->dict) out.push_back(kv.first);
    return out;
}

std::string Env::leafBytes(const Value* v, const std::string& leaf) {
    if (leaf == "data") return strForData(*v) + "\n";
    if (leaf == "status") {
        std::string n = v->container()
            ? std::to_string(v->k == Value::LIST ? v->list.size() : v->dict.size()) : "-";
        return "type=" + std::string(v->tname()) + " len=" + n +
               " id=" + std::to_string((uintptr_t)v) + "\n";
    }
    if (leaf == "ctl") return "# append <literal> | set <key> <literal> | del <key>\n";
    throw UnipathError("ENOENT");
}

// ast.literal_eval 的最小子集：整數、單/雙引號字串，其餘退化為原字串。
Value Env::parseLiteral(const std::string& text) {
    size_t a = text.find_first_not_of(" \t\r\n");
    size_t b = text.find_last_not_of(" \t\r\n");
    std::string t = (a == std::string::npos) ? "" : text.substr(a, b - a + 1);
    if (!t.empty()) {
        size_t i = (t[0] == '-' || t[0] == '+') ? 1 : 0;
        bool digits = i < t.size();
        for (; i < t.size(); ++i) if (!isdigit((unsigned char)t[i])) { digits = false; break; }
        if (digits) { try { return Value::Int(std::stoll(t)); } catch (...) {} }
        if (t.size() >= 2 && (t.front() == '\'' || t.front() == '"') && t.back() == t.front())
            return Value::Str(t.substr(1, t.size() - 2));
    }
    return Value::Str(t);
}

StatInfo Env::stat(const std::string& path) {
    std::lock_guard<std::mutex> g(lock_);
    Value *parent, *node; std::string leaf;
    resolve(path, parent, node, leaf);
    if (leaf.empty()) return {true, 0, false};
    return {false, leafBytes(node, leaf).size(), leaf == "status"};
}

std::vector<std::string> Env::readdir(const std::string& path) {
    std::lock_guard<std::mutex> g(lock_);
    Value *parent, *node; std::string leaf;
    resolve(path, parent, node, leaf);
    if (!leaf.empty()) throw UnipathError("ENOTDIR");
    std::vector<std::string> out = {"data", "ctl", "status"};
    for (auto& c : children(node)) out.push_back(c);
    return out;
}

std::string Env::read(const std::string& path) {
    std::lock_guard<std::mutex> g(lock_);
    Value *parent, *node; std::string leaf;
    resolve(path, parent, node, leaf);
    if (leaf.empty()) throw UnipathError("EISDIR");
    return leafBytes(node, leaf);
}

size_t Env::write(const std::string& path, const std::string& data) {
    std::lock_guard<std::mutex> g(lock_);
    Value *parent, *node; std::string leaf;
    resolve(path, parent, node, leaf);
    if (leaf == "data") {
        if (parent == nullptr) throw UnipathError("EROFS");
        *node = parseLiteral(data);
        return data.size();
    }
    if (leaf == "ctl") return data.size();   // ctl 動詞：此埠暫接收即忽略（見 README 取捨）
    throw UnipathError("EROFS");
}
