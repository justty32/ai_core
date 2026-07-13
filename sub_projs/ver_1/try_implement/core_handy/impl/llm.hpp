// impl/llm.hpp — LLM backend：OpenAI 相容 chat-completions 呼叫
//
// 串起前面三磚：ac::detail::json_str（組請求·escape）+ ac::http::post（送）+ ac::json::parse（抽回應）。
// 對應 Python 線 OpenAIBackend。OpenAI 相容端點涵蓋本地（ollama/llama.cpp/vLLM）與多數雲端（OpenRouter）。
// 形狀＝roadmap 函式模型：prompt(string) → completion(string)。
// 註：Anthropic /v1/messages 形狀不同，留作後續 backend。
#pragma once

#include <stdexcept>
#include <string>

#include "http.hpp"
#include "json.hpp"
#include "meta_json.hpp"  // 借 ac::detail::json_str 做 JSON 字串 escape（不重造）

namespace ac::llm {

struct Config {
  std::string base_url;   // 至 /v1（如 http://localhost:11434/v1、https://openrouter.ai/api/v1）
  std::string model;      // 模型名
  std::string api_key;    // 選填：有則加 Authorization: Bearer
};

struct Reply {
  std::string content;       // 模型回覆內容
  long long total_tokens = 0;  // usage.total_tokens（0=未提供）；餵軸 5 rate-meter
};

// 單輪對話：prompt → Reply（content + token 用量）。HTTP 非 2xx 或無 content 即丟例外。
inline Reply chat(const Config& cfg, const std::string& prompt) {
  const std::string body =
      "{\"model\":" + ac::detail::json_str(cfg.model) +
      ",\"messages\":[{\"role\":\"user\",\"content\":" + ac::detail::json_str(prompt) +
      "}]}";

  ac::http::Headers h = {{"Content-Type", "application/json"}};
  if (!cfg.api_key.empty()) h.push_back({"Authorization", "Bearer " + cfg.api_key});

  const ac::http::Response r = ac::http::post(cfg.base_url + "/chat/completions", body, h);
  if (r.status < 200 || r.status >= 300)
    throw std::runtime_error("ac::llm: HTTP " + std::to_string(r.status) + "：" + r.body);

  const ac::json::Value j = ac::json::parse(r.body);
  const ac::json::Value& c = j["choices"][0]["message"]["content"];
  if (!c.is_valid())
    throw std::runtime_error("ac::llm: 回應無 choices[0].message.content：" + r.body);

  Reply rep;
  rep.content = c.as_string();
  rep.total_tokens = static_cast<long long>(j["usage"]["total_tokens"].as_number(0));
  return rep;
}

}  // namespace ac::llm
