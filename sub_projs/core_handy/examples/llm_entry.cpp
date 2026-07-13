// examples/llm_entry.cpp — 元件1 LLM Entry Manager（C++ 版）
//
// 統一 LLM 呼叫入口。把 http+json+llm+rate+serve 拼成 roadmap 元件 1：
//   - one-shot：prompt(stdin) → completion(stdout)
//   - --serve <sock>：長駐 daemon，循序佇列化（LLM 單資源），共用一個 RateMeter 跨呼叫累計 consume-rate
// config 由環境變數（同 Python 線慣例）：
//   AI_CORE_LLM_BASE_URL（至 /v1）、AI_CORE_LLM_MODEL、AI_CORE_LLM_API_KEY（選填）
//
// 軸 9：呼叫 LLM＝不確定（未認證）→ 宣告 uncertainty>0；軸 5：需網路。
#include <cstdlib>
#include <iostream>
#include <string>

#include "ac_helper.hpp"

namespace {

ac::llm::Config config_from_env() {
  auto env = [](const char* k, const char* def) {
    const char* v = std::getenv(k);
    return std::string(v ? v : def);
  };
  return {env("AI_CORE_LLM_BASE_URL", "http://localhost:11434/v1"),
          env("AI_CORE_LLM_MODEL", "qwen2.5"),
          env("AI_CORE_LLM_API_KEY", "")};
}

}  // namespace

int main(int argc, char** argv) {
  // 自我描述：prompt 進 / completion 出（text、batch）；需網路；呼叫 LLM 故 uncertainty>0。
  ac::Meta meta;
  meta.entries["prompt"]     = {ac::Entry::in,  ac::Entry::batch, ac::Entry::text};
  meta.entries["completion"] = {ac::Entry::out, ac::Entry::batch, ac::Entry::text};
  meta.resources.network = ac::Network{};   // 有值＝需網路
  meta.uncertainty = 50;                     // 軸 9：未認證的隨機環節（馴化可降）
  ac::intercept(argc, argv, meta);

  const ac::llm::Config cfg = config_from_env();
  ac::rate::Meter meter;                      // 單例計量，跨請求累計

  // ── server 模式：LLM 單資源 → 循序佇列；共用 RateMeter 累計 consume-rate。
  if (const std::string sock = ac::cli::resolve(argc, argv, "--serve", ""); !sock.empty()) {
    std::cerr << "llm_entry serving on " << sock << " model=" << cfg.model << "\n";
    ac::serve::serve_socket(sock, [&](std::string prompt) {
      const ac::llm::Reply rep = ac::llm::chat(cfg, prompt);
      meter.add(rep.total_tokens);            // 跨呼叫累計
      std::cerr << "[meter] " << meter.report() << "\n";
      return rep.content;
    });
    return 0;
  }

  // ── one-shot 模式：prompt → completion。
  const ac::llm::Reply rep = ac::llm::chat(cfg, ac::io::read_all("-"));
  meter.add(rep.total_tokens);
  ac::io::write_all("-", rep.content);
  std::cerr << "[meter] " << meter.report() << "\n";
  return 0;
}
