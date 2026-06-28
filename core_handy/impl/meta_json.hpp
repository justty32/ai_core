// impl/meta_json.hpp — 膠水：Meta → --metadata JSON 序列化
//
// 三層通則塑形 JSON（見 notes/00_index.md）：固定欄位恆出、opt 有才出。
// 2026-06-28：extra 收斂為單一 `Meta::extra` → 序列化成頂層 "extra":{...}（有值才出）。
// 純標準庫、手寫最小 emitter（無 JSON 相依，符合 least-dependency）。
#pragma once

#include <string>
#include <vector>

#include "../defs/axes.hpp"

namespace ac {

namespace detail {

// 最小 JSON 字串轉義。
inline std::string json_str(const std::string& s) {
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

// resources 的子物件：只輸出有值的 opt 欄位。
inline std::string emit_resources(const Resources& r) {
  std::vector<std::string> parts;
  auto kv = [](const char* k, const std::optional<std::string>& v) {
    return v ? std::string("\"") + k + "\":" + json_str(*v) : std::string();
  };
  auto sub = [&](const char* key, std::initializer_list<std::string> fields) {
    std::string body;
    for (const auto& f : fields) {
      if (f.empty()) continue;
      if (!body.empty()) body += ",";
      body += f;
    }
    parts.push_back(std::string("\"") + key + "\":{" + body + "}");
  };

  if (r.memory)  sub("memory", {kv("startup", r.memory->startup),
                                 kv("peak", r.memory->peak),
                                 kv("idle", r.memory->idle)});
  if (r.gpu)     sub("gpu", {kv("vram", r.gpu->vram)});
  if (r.time)    sub("time", {kv("expected", r.time->expected),
                              kv("max", r.time->max)});
  if (r.disk)    parts.push_back(std::string("\"disk\":") + json_str(*r.disk));
  if (r.network) sub("network", {kv("traffic", r.network->traffic)});

  std::string out = "{";
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) out += ",";
    out += parts[i];
  }
  out += "}";
  return out;
}

}  // namespace detail

// 把 Meta 序列化成 --metadata JSON 字串（單行）。
inline std::string to_metadata_json(const Meta& m) {
  using detail::json_str;
  std::vector<std::string> fields;

  // 軸 1 entries（固定恆出；每個 entry 的 direction/content 恆出）。
  {
    std::string e = "\"entries\":{";
    bool first = true;
    for (const auto& [name, en] : m.entries) {
      if (!first) e += ",";
      first = false;
      e += json_str(name) + ":{\"direction\":" + std::to_string(en.direction) +
           ",\"content\":" + std::to_string(en.content) + "}";
    }
    e += "}";
    fields.push_back(e);
  }

  // 軸 2 persistent（固定 bool，內聯）。
  fields.push_back(std::string("\"persistent\":") + (m.persistent ? "true" : "false"));

  // 軸 3 stateful（固定 bool）。
  fields.push_back(std::string("\"stateful\":") + (m.stateful ? "true" : "false"));

  // 軸 5 resources（opt 有才出）。
  fields.push_back(std::string("\"resources\":") + detail::emit_resources(m.resources));

  // 軸 6 interruptible（level 固定）。
  fields.push_back(std::string("\"interruptible\":{\"level\":") +
                   std::to_string(m.interruptible.level) + "}");

  // 軸 7 guarantee（固定，內聯）。
  fields.push_back(std::string("\"guarantee\":") +
                   std::to_string(static_cast<unsigned>(m.guarantee)));

  // 軸 8 dry_run（固定 bool）。
  fields.push_back(std::string("\"dry_run\":") + (m.allow_dry_run ? "true" : "false"));

  // 軸 9 uncertainty（固定，內聯）。
  fields.push_back(std::string("\"uncertainty\":") + std::to_string(m.uncertainty));

  // ★ 全軸共用的單一 extra 袋（軸-無關，有值才出，序列化為頂層 "extra":{...}）。
  if (m.extra && !m.extra->empty()) {
    std::string s = "\"extra\":{";
    bool first = true;
    for (const auto& [k, v] : *m.extra) {
      if (!first) s += ",";
      first = false;
      s += json_str(k) + ":" + json_str(v);
    }
    s += "}";
    fields.push_back(s);
  }

  std::string out = "{";
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i) out += ",";
    out += fields[i];
  }
  out += "}";
  return out;
}

}  // namespace ac
