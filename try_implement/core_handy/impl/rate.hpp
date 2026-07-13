// impl/rate.hpp — 軸 5 resources：consume-rate 計量（RateMeter）
//
// roadmap：LLM 是單例資源，集中管理 consume rate（token / 錢 / 本地 GPU）以避免超賣。
// 對應 Python 線 RateMeter。本 v0 計量 token 與（呼叫方算好的）花費 + 請求數，並可設 token 上限。
// 用法：entry manager（長駐 daemon）持有一個 Meter，跨所有請求累計（warm process 單例）。
// GPU 計量：API 回應通常不含 → 暫不計（留註，需本地 runtime 才量得到）。
#pragma once

#include <string>

namespace ac::rate {

struct Meter {
  long long requests = 0;       // 累計請求數
  long long tokens = 0;         // 累計 token
  double cost = 0.0;            // 累計花費（呼叫方按單價算後加入）
  long long token_budget = 0;   // >0 即上限；達到後 over_budget() 為真（配額/避免超賣）

  // 記一次呼叫的消耗。
  void add(long long toks, double call_cost = 0.0) {
    ++requests;
    tokens += toks;
    cost += call_cost;
  }

  // 是否已達 token 配額（token_budget<=0 視為無限）。
  bool over_budget() const { return token_budget > 0 && tokens >= token_budget; }

  // 零相依 JSON 報告（手寫，no-wheel-remake）。
  std::string report() const {
    return "{\"requests\":" + std::to_string(requests) +
           ",\"tokens\":" + std::to_string(tokens) +
           ",\"cost\":" + std::to_string(cost) +
           ",\"over_budget\":" + (over_budget() ? "true" : "false") + "}";
  }
};

}  // namespace ac::rate
