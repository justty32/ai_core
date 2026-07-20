// test_classify.cpp —— 離線單元測試（不觸網、不需憑證、不起 proxy）。
// 只驗 app::classify_error 的三類分流，對齊姊妹專案 janet-try-1/test/basic.janet。
//
// 編＋跑：./build.sh test   （或 g++ -std=c++23 -Isrc test/test_classify.cpp $(pkg-config --cflags --libs cllm)）
#include "app.hpp"

#include <cstdio>
#include <string_view>

namespace {
int fails = 0;

void expect(std::string_view msg, app::Kind want) {
  app::Triage t = app::classify_error(msg);
  bool ok = (t.kind == want);
  if (!ok) ++fails;
  std::printf("  [%s] classify(\"%.*s\") → %s（期望 %s）\n", ok ? "OK" : "FAIL",
              static_cast<int>(msg.size()), msg.data(), app::kind_name(t.kind),
              app::kind_name(want));
}
} // namespace

int main() {
  std::puts("cpp-try-1 離線分流測試：");

  // :auth —— HTTP 401 / authentication / 未授權 / 缺 authorization
  expect("後端錯誤 (HTTP 401): Unauthorized", app::Kind::Auth);
  expect("authentication_error: invalid x-api-key", app::Kind::Auth);
  expect("缺 Authorization header", app::Kind::Auth);
  expect("未登入", app::Kind::Auth);

  // :sidecar —— connection refused 家族
  expect("curl: (7) Failed to connect: Connection refused", app::Kind::Sidecar);
  expect("could not connect to 127.0.0.1:8787", app::Kind::Sidecar);
  expect("connection reset by peer", app::Kind::Sidecar);

  // :other —— 其餘
  expect("something else broke", app::Kind::Other);
  expect("HTTP 500 internal server error", app::Kind::Other);

  // 退出碼語意（sidecar=3，其餘一般錯誤=1，Ok=0）
  if (app::exit_code_for(app::Kind::Sidecar) != 3) { std::puts("  [FAIL] sidecar 退出碼應為 3"); ++fails; }
  if (app::exit_code_for(app::Kind::Auth) != 1) { std::puts("  [FAIL] auth 退出碼應為 1"); ++fails; }
  if (app::exit_code_for(app::Kind::Ok) != 0) { std::puts("  [FAIL] ok 退出碼應為 0"); ++fails; }

  std::printf("結果：%s\n", fails == 0 ? "全綠 ✓" : "有失敗 ✗");
  return fails == 0 ? 0 : 1;
}
