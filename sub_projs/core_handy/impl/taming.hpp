// impl/taming.hpp — 軸 9 nondeterministic：馴化原語（把不可靠 f(str)→str 收成可靠）
//
// roadmap 願景頂 / try_implement/docs/llm_taming_framework.md 的 C++ KISS 濃縮。
// 本輪只落「蒙地卡羅 + 打分」兩支 L3 聚合組合子——同輸入抽 N 次、用統計把方差換穩定：
//   - vote(f, n)         ：self-consistency 多數決（蒙地卡羅）；同答案抽多了會浮上來。
//   - best_of(f, n, sc)  ：抽 N 取最高分（打分）；冷熱分工＝f 熱(多生)、score 冷(嚴篩)。
// 兩者共用 sample() 核心。retry/guard/memoize 等其他層待真消費者逼出（見 notes/axis_9）。
//
// 形狀：組合子（decorator）——吃一個 f(str)→str 回一個新的 f(str)→str，故可層層疊：
//   auto reliable = best_of(vote(chat, 3), 5, score);
// 被馴化的對象＝隨機環節（Meta::uncertainty>0），通常是 llm::chat 綁好 prompt 後的 f。
// 馴化的 telos：把 uncertainty 往 0 推（債務儀表）；本層是「降方差」那段力。
#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ac::taming {

// 被馴化的函式型態：一切皆 f(str)→str（ai_core 的核心抽象）。
using Fn    = std::function<std::string(const std::string&)>;
// 打分器：輸出→分數（高＝好）。可確定（編譯過否、長度…）或另一個 LLM(critic)。
using Score = std::function<double(const std::string&)>;

// 蒙地卡羅核心：同輸入跑 n 次、蒐集所有輸出（n<=0 → 空）。vote/best_of 共用。
inline std::vector<std::string> sample(const Fn& f, const std::string& in, int n) {
  std::vector<std::string> outs;
  if (n > 0) outs.reserve(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) outs.push_back(f(in));
  return outs;
}

// vote：self-consistency 多數決。抽 n 次取出現最多的輸出；平手取先抽到者。
// n=1 退化成原函式；n<=0 回空字串。
inline Fn vote(Fn f, int n) {
  return [f = std::move(f), n](const std::string& in) -> std::string {
    std::map<std::string, int> tally;
    std::string best;
    int best_count = 0;
    for (const auto& o : sample(f, in, n)) {
      int c = ++tally[o];
      if (c > best_count) { best_count = c; best = o; }  // > 而非 >=：平手保留先到者
    }
    return best;
  };
}

// best_of：打分選優。抽 n 次、用 score 打分、取最高分者；平手取先抽到者。
// n<=0 回空字串。
inline Fn best_of(Fn f, int n, Score score) {
  return [f = std::move(f), n, score = std::move(score)](const std::string& in) -> std::string {
    std::string best;
    double best_score = 0.0;
    bool have = false;
    for (const auto& o : sample(f, in, n)) {
      const double s = score(o);
      if (!have || s > best_score) { have = true; best_score = s; best = o; }
    }
    return best;
  };
}

}  // namespace ac::taming
