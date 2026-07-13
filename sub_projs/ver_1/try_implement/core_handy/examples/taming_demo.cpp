// examples/taming_demo.cpp — 軸 9 馴化原語端到端驗證（不需真 LLM）
//
// 用「確定性的 flaky f」模擬隨機環節：把它包進 vote / best_of，證明統計能把方差換穩定。
// 這對應 Python 線 lib_smoke_test 用 ScriptedBackend 驗 compose 的做法——不打 API、可重現。
//
// 自帶斷言，回傳碼 0＝全綠。CMake target: taming_demo。
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "ac_helper.hpp"

int main() {
  // ── 場景：一個「隨機」環節，5 次抽樣回 [A, B, A, A, C]。
  //    多數是 A（3/5），但單看第 2 抽會拿到 B。馴化＝抽多次、用統計穩住。
  std::vector<std::string> scripted = {"A", "B", "A", "A", "C"};
  int call = 0;
  ac::taming::Fn flaky = [&](const std::string&) {
    return scripted[static_cast<size_t>(call++) % scripted.size()];
  };

  // ── vote（蒙地卡羅多數決）：抽 5 次取最多 → A。
  call = 0;
  std::string v = ac::taming::vote(flaky, 5)("任意輸入");
  std::cout << "vote(5) = " << v << "  (期望 A)\n";
  assert(v == "A");

  // n=1 退化成原函式（第一抽＝A）。
  call = 0;
  assert(ac::taming::vote(flaky, 1)("x") == "A");

  // 平手取先抽到者：抽 2 次得 [A,B] 各 1 票 → 先到的 A。
  call = 0;
  assert(ac::taming::vote(flaky, 2)("x") == "A");

  // ── best_of（打分選優）：score=字串長度，讓較長者勝出。
  //    候選回 ["x","yyy","zz"]，best_of 取 "yyy"（最長＝最高分）。
  std::vector<std::string> cand = {"x", "yyy", "zz"};
  int c2 = 0;
  ac::taming::Fn gen = [&](const std::string&) {
    return cand[static_cast<size_t>(c2++) % cand.size()];
  };
  auto score_len = [](const std::string& s) { return static_cast<double>(s.size()); };
  std::string b = ac::taming::best_of(gen, 3, score_len)("任意輸入");
  std::cout << "best_of(3, len) = " << b << "  (期望 yyy)\n";
  assert(b == "yyy");

  // ── 組合子可疊：best_of 包在 vote 外（先各自多數決、再打分選）。
  //    這裡只證型別接得起來、能跑（語意組合留給真消費者）。
  call = 0;
  auto stacked = ac::taming::best_of(ac::taming::vote(flaky, 3), 2, score_len);
  std::string out = stacked("x");
  std::cout << "best_of(vote(flaky,3),2,len) = " << out << "  (能跑即可)\n";
  assert(!out.empty());

  // ── 邊界：n<=0 回空字串（不崩）。
  assert(ac::taming::vote(flaky, 0)("x").empty());
  assert(ac::taming::best_of(gen, 0, score_len)("x").empty());

  std::cout << "\n[taming_demo] 全部斷言通過 ✅\n";
  return 0;
}
