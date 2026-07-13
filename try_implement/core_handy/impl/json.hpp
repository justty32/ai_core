// impl/json.hpp — 最小零相依 JSON parser（為抽 LLM 回應欄位）
//
// 標準庫無 JSON parser（meta_json 只有 emitter）。這裡補一個遞迴下降的最小取值器，
// 足以走訪 LLM 回應如 `choices[0].message.content` / `content[0].text`。
// 設計：鏈式取值安全——走訪不存在的 key/index 回 Invalid 哨兵（不丟例外），
//   末端 as_string("") 給預設值，故 `j["a"][3]["b"].as_string()` 不會崩。
// 支援：object / array / string（含 \uXXXX 與代理對）/ number / bool / null。
#pragma once

#include <cctype>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace ac::json {

class Value {
 public:
  enum class T { Null, Bool, Num, Str, Arr, Obj, Invalid };

  T t = T::Null;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<Value> a;
  std::map<std::string, Value> o;

  static const Value& invalid() {
    static const Value v = [] { Value x; x.t = T::Invalid; return x; }();
    return v;
  }

  // 物件取 key；非物件或缺 key → Invalid 哨兵。
  const Value& operator[](const std::string& key) const {
    if (t == T::Obj) {
      const auto it = o.find(key);
      if (it != o.end()) return it->second;
    }
    return invalid();
  }
  // 陣列取 index；非陣列或越界 → Invalid 哨兵。
  const Value& operator[](std::size_t i) const {
    if (t == T::Arr && i < a.size()) return a[i];
    return invalid();
  }

  std::string as_string(const std::string& def = "") const { return t == T::Str ? s : def; }
  double as_number(double def = 0) const { return t == T::Num ? n : def; }
  bool as_bool(bool def = false) const { return t == T::Bool ? b : def; }
  bool is_valid() const { return t != T::Invalid; }
  bool is_null() const { return t == T::Null; }
};

namespace detail {

inline void append_utf8(std::string& out, unsigned cp) {
  if (cp <= 0x7F) {
    out += static_cast<char>(cp);
  } else if (cp <= 0x7FF) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp <= 0xFFFF) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

struct Parser {
  const std::string& s;
  std::size_t i = 0;
  bool err = false;

  explicit Parser(const std::string& str) : s(str) {}

  void ws() {
    while (i < s.size() &&
           (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
      ++i;
  }
  char peek() { return i < s.size() ? s[i] : '\0'; }

  unsigned hex4() {
    unsigned v = 0;
    for (int k = 0; k < 4 && i < s.size(); ++k) {
      const char c = s[i++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
      else { err = true; }
    }
    return v;
  }

  std::string parse_str_raw() {
    std::string out;
    ++i;  // 跳開頭 "
    while (i < s.size()) {
      const char c = s[i++];
      if (c == '"') return out;
      if (c == '\\') {
        if (i >= s.size()) break;
        const char e = s[i++];
        switch (e) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/': out += '/'; break;
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': {
            unsigned cp = hex4();
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() &&
                s[i] == '\\' && s[i + 1] == 'u') {  // 高代理 → 取低代理組成
              i += 2;
              const unsigned lo = hex4();
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            }
            append_utf8(out, cp);
            break;
          }
          default: out += e; break;
        }
      } else {
        out += c;
      }
    }
    err = true;  // 未見收尾 "
    return out;
  }

  Value parse_value() {
    ws();
    const char c = peek();
    if (c == '"') { Value v; v.t = Value::T::Str; v.s = parse_str_raw(); return v; }
    if (c == '{') return parse_object();
    if (c == '[') return parse_array();
    if (c == 't' || c == 'f') return parse_bool();
    if (c == 'n') { i += 4; Value v; v.t = Value::T::Null; return v; }  // null
    return parse_number();
  }

  Value parse_bool() {
    Value v; v.t = Value::T::Bool;
    if (s.compare(i, 4, "true") == 0) { v.b = true; i += 4; }
    else if (s.compare(i, 5, "false") == 0) { v.b = false; i += 5; }
    else err = true;
    return v;
  }

  Value parse_number() {
    const std::size_t start = i;
    while (i < s.size() &&
           (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '-' ||
            s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E'))
      ++i;
    if (i == start) { err = true; return Value::invalid(); }
    Value v; v.t = Value::T::Num;
    v.n = std::strtod(s.c_str() + start, nullptr);
    return v;
  }

  Value parse_array() {
    Value v; v.t = Value::T::Arr;
    ++i;  // [
    ws();
    if (peek() == ']') { ++i; return v; }
    for (;;) {
      v.a.push_back(parse_value());
      ws();
      const char c = peek();
      if (c == ',') { ++i; continue; }
      if (c == ']') { ++i; break; }
      err = true; break;
    }
    return v;
  }

  Value parse_object() {
    Value v; v.t = Value::T::Obj;
    ++i;  // {
    ws();
    if (peek() == '}') { ++i; return v; }
    for (;;) {
      ws();
      if (peek() != '"') { err = true; break; }
      const std::string key = parse_str_raw();
      ws();
      if (peek() != ':') { err = true; break; }
      ++i;
      v.o[key] = parse_value();
      ws();
      const char c = peek();
      if (c == ',') { ++i; continue; }
      if (c == '}') { ++i; break; }
      err = true; break;
    }
    return v;
  }
};

}  // namespace detail

// 解析整個 JSON 文字 → Value。失敗回 Invalid 哨兵。
inline Value parse(const std::string& text) {
  detail::Parser p(text);
  Value v = p.parse_value();
  return p.err ? Value::invalid() : v;
}

}  // namespace ac::json
