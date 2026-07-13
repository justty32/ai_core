// impl_remake/json.hpp — 最小 JSON emitter 原語（Meta 無關）
//
// 從 meta_json.hpp 獨立出來的 JSON 塑形操作：字串轉義、片段串接、object 包裝、
// "key":value 片段。只管「怎麼拼合法 JSON」，不認識任何 defs 型別。
// 純標準庫、手寫最小 emitter（無 JSON 相依，符合 least-dependency）。
//
// 模組形狀參考 impl/json.hpp（同掛 namespace ac::json）：那邊是 parser
// （抽 LLM 回應欄位），本檔是 emitter 半邊，兩者互補、可並存。
#pragma once

#include <string>
#include <vector>

namespace ac::json {

// 最小 JSON 字串轉義。
inline std::string str(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\t': out += "\\t";  break;
      case '\r': out += "\\r";  break;
      default:   out += c;      break;
    }
  }
  out += "\"";
  return out;
}

// 以 sep 串接片段（取代各處重複的手寫 join 迴圈）。
inline std::string join(const std::vector<std::string>& parts, char sep) {
  std::string out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) out += sep;
    out += parts[i];
  }
  return out;
}

// 把片段包成 JSON object：{f1,f2,...}。
inline std::string obj(const std::vector<std::string>& fields) {
  return "{" + join(fields, ',') + "}";
}

// "key":value 片段（value 已是合法 JSON 字面）。
inline std::string field(const char* key, const std::string& json_value) {
  return str(key) + ":" + json_value;
}

}  // namespace ac::json
