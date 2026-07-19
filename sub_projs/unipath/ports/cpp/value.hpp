// value.hpp — 活執行態環境的值模型（對應 Python 的 int/str/list/dict）。
// 逐位元組相容 unipath_env.py：data 葉子渲染需與 Python repr()/str() 對齊。
#pragma once
#include <string>
#include <vector>
#include <utility>

struct Value {
    enum K { INT, STR, LIST, DICT } k;
    long long i = 0;
    std::string s;
    std::vector<Value> list;
    std::vector<std::pair<std::string, Value>> dict;  // 有序，模擬 Python dict 插入序

    static Value Int(long long v) { Value x; x.k = INT; x.i = v; return x; }
    static Value Str(std::string v) { Value x; x.k = STR; x.s = std::move(v); return x; }
    bool container() const { return k == LIST || k == DICT; }
    const char* tname() const {
        switch (k) { case INT: return "int"; case STR: return "str";
                     case LIST: return "list"; default: return "dict"; }
    }
};

// repr()：巢狀情境用（字串加單引號），對齊 Python repr。
inline std::string reprValue(const Value& v) {
    switch (v.k) {
        case Value::INT: return std::to_string(v.i);
        case Value::STR: return "'" + v.s + "'";
        case Value::LIST: {
            std::string o = "[";
            for (size_t j = 0; j < v.list.size(); ++j) {
                if (j) o += ", ";
                o += reprValue(v.list[j]);
            }
            return o + "]";
        }
        default: {
            std::string o = "{";
            for (size_t j = 0; j < v.dict.size(); ++j) {
                if (j) o += ", ";
                o += "'" + v.dict[j].first + "': " + reprValue(v.dict[j].second);
            }
            return o + "}";
        }
    }
}

// data 葉子：容器用 repr，純量用 str（字串直接輸出、整數輸出數字）。
inline std::string strForData(const Value& v) {
    if (v.container()) return reprValue(v);
    return v.k == Value::STR ? v.s : std::to_string(v.i);
}
