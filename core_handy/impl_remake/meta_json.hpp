// impl_remake/meta_json.hpp — defs 的實現（其一）：Meta → --metadata JSON 序列化
//
// 「defs 的實現」＝把純描述型別 ac::Meta 變成可觀察行為。本檔是其核心：
// 將一個 Meta 實例塑形成 --metadata 應輸出的 JSON 字串。
//
// 三層塑形規則（沿用、不改——此 JSON 形狀是跨元件硬契約，Hub/Indexer 靠它）：
//   1. 固定欄位恆出：entries / persistent / stateful / resources / interruptible
//      / guarantee / dry_run / uncertainty 永遠出現（即使是預設/空）。
//   2. opt 有才出：resources 內各子物件與其欄位，只在有值時出現。
//   3. extra 收頂層：Meta::extra 有值才序列化成頂層 "extra":{...}。
//
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
  return json_str(key) + ":" + json_value;
}

// resources 子物件：只輸出有值的 opt 欄位。
inline std::string emit_resources(const Resources& r) {
  // 收集有值的 "k":"v" 片段；空 opt 略過。
  auto kv = [](const char* k, const std::optional<std::string>& v) -> std::optional<std::string> {
    if (!v) return std::nullopt;
    return field(k, json_str(*v));
  };
  // 把若干可選片段組成子物件 "key":{...}（無論子物件本身是否空，呼叫端已先判 has-value）。
  auto sub = [](const char* key, std::initializer_list<std::optional<std::string>> kvs) {
    std::vector<std::string> body;
    for (const auto& f : kvs)
      if (f) body.push_back(*f);
    return field(key, obj(body));
  };

  std::vector<std::string> parts;
  if (r.memory)  parts.push_back(sub("memory", {kv("startup", r.memory->startup),
                                                 kv("peak", r.memory->peak),
                                                 kv("idle", r.memory->idle)}));
  if (r.gpu)     parts.push_back(sub("gpu", {kv("vram", r.gpu->vram)}));
  if (r.time)    parts.push_back(sub("time", {kv("expected", r.time->expected),
                                              kv("max", r.time->max)}));
  if (r.disk)    parts.push_back(field("disk", json_str(*r.disk)));
  if (r.network) parts.push_back(sub("network", {kv("traffic", r.network->traffic)}));

  return obj(parts);
}

// 軸 1 entries：每個 entry 的 direction/flow/content 恆出。
inline std::string emit_entries(const Entries& entries) {
  std::vector<std::string> parts;
  for (const auto& [name, en] : entries) {
    parts.push_back(field(name.c_str(), obj({
        field("direction", std::to_string(en.direction)),
        field("flow",      std::to_string(en.flow)),
        field("content",   std::to_string(en.content)),
    })));
  }
  return obj(parts);
}

// 軸-無關的單一 extra 袋：有值才出。回空字串表示「不出此欄位」。
inline std::string emit_extra(const Extra& extra) {
  if (!extra || extra->empty()) return std::string();
  std::vector<std::string> parts;
  for (const auto& [k, v] : *extra)
    parts.push_back(field(k.c_str(), json_str(v)));
  return field("extra", obj(parts));
}

}  // namespace detail

// 把 Meta 序列化成 --metadata JSON 字串（單行）。
inline std::string to_metadata_json(const Meta& m) {
  using namespace detail;
  std::vector<std::string> fields = {
      field("entries", emit_entries(m.entries)),                                  // 軸 1
      field("persistent", m.persistent ? "true" : "false"),                       // 軸 2
      field("stateful", m.stateful ? "true" : "false"),                           // 軸 3
      field("resources", emit_resources(m.resources)),                            // 軸 5
      field("interruptible", obj({field("level", std::to_string(m.interruptible.level))})),  // 軸 6
      field("guarantee", std::to_string(static_cast<unsigned>(m.guarantee))),     // 軸 7
      field("dry_run", m.allow_dry_run ? "true" : "false"),                       // 軸 8
      field("uncertainty", std::to_string(m.uncertainty)),                        // 軸 9
  };
  // ★ 單一 extra 袋（有值才出，序列化為頂層 "extra":{...}）。
  if (auto e = emit_extra(m.extra); !e.empty())
    fields.push_back(e);

  return obj(fields);
}

}  // namespace ac
