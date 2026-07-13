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
// JSON 塑形原語（轉義/join/obj/field）已獨立至 json.hpp（namespace ac::json）；
// 本檔只剩 Meta 專屬的塑形邏輯。
#pragma once

#include <string>
#include <vector>

#include "../defs/axes.hpp"
#include "json.hpp"

namespace ac {

namespace detail {

// resources 子物件：只輸出有值的 opt 欄位。
inline std::string emit_resources(const Resources& r) {
  // 收集有值的 "k":"v" 片段；空 opt 略過。
  auto kv = [](const char* k, const std::optional<std::string>& v) -> std::optional<std::string> {
    if (!v) return std::nullopt;
    return json::field(k, json::str(*v));
  };
  // 把若干可選片段組成子物件 "key":{...}（無論子物件本身是否空，呼叫端已先判 has-value）。
  auto sub = [](const char* key, std::initializer_list<std::optional<std::string>> kvs) {
    std::vector<std::string> body;
    for (const auto& f : kvs)
      if (f) body.push_back(*f);
    return json::field(key, json::obj(body));
  };

  std::vector<std::string> parts;
  if (r.memory)  parts.push_back(sub("memory", {kv("startup", r.memory->startup),
                                                 kv("peak", r.memory->peak),
                                                 kv("idle", r.memory->idle)}));
  if (r.gpu)     parts.push_back(sub("gpu", {kv("vram", r.gpu->vram)}));
  if (r.time)    parts.push_back(sub("time", {kv("expected", r.time->expected),
                                              kv("max", r.time->max)}));
  if (r.disk)    parts.push_back(json::field("disk", json::str(*r.disk)));
  if (r.network) parts.push_back(sub("network", {kv("traffic", r.network->traffic)}));

  return json::obj(parts);
}

// 軸 1 entries：每個 entry 的 direction/flow/content 恆出。
inline std::string emit_entries(const Entries& entries) {
  std::vector<std::string> parts;
  for (const auto& [name, en] : entries) {
    parts.push_back(json::field(name.c_str(), json::obj({
        json::field("direction", std::to_string(en.direction)),
        json::field("flow",      std::to_string(en.flow)),
        json::field("content",   std::to_string(en.content)),
    })));
  }
  return json::obj(parts);
}

// 軸-無關的單一 extra 袋：有值才出。回空字串表示「不出此欄位」。
inline std::string emit_extra(const Extra& extra) {
  if (!extra || extra->empty()) return std::string();
  std::vector<std::string> parts;
  for (const auto& [k, v] : *extra)
    parts.push_back(json::field(k.c_str(), json::str(v)));
  return json::field("extra", json::obj(parts));
}

}  // namespace detail

// 把 Meta 序列化成 --metadata JSON 字串（單行）。
inline std::string to_metadata_json(const Meta& m) {
  using namespace detail;
  std::vector<std::string> fields = {
      json::field("entries", emit_entries(m.entries)),                                  // 軸 1
      json::field("persistent", m.persistent ? "true" : "false"),                       // 軸 2
      json::field("stateful", m.stateful ? "true" : "false"),                           // 軸 3
      json::field("resources", emit_resources(m.resources)),                            // 軸 5
      json::field("interruptible",
                  json::obj({json::field("level", std::to_string(m.interruptible.level))})),  // 軸 6
      json::field("guarantee", std::to_string(static_cast<unsigned>(m.guarantee))),     // 軸 7
      json::field("dry_run", m.allow_dry_run ? "true" : "false"),                       // 軸 8
      json::field("uncertainty", std::to_string(m.uncertainty)),                        // 軸 9
  };
  // ★ 單一 extra 袋（有值才出，序列化為頂層 "extra":{...}）。
  if (auto e = emit_extra(m.extra); !e.empty())
    fields.push_back(e);

  return json::obj(fields);
}

}  // namespace ac
