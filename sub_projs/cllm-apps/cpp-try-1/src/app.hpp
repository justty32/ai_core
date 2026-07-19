#pragma once
// app.hpp —— cpp-try-1 核心模組（純邏輯，main.cpp 與 test 都 include 這個）。
//
// 對應姊妹專案 janet-try-1/init.janet，只做兩件事：
//   1. 包一層 ask()：呼 cllm C++ binding 的 llm::Client::ask（<cllm/llm.hpp>），
//      把 std::expected<T,Error> 的失敗照 tools/INTEGRATION.md 分流。
//   2. classify_error：把後端錯誤字串分成三類：
//         401 / authentication  → 憑證問題（沒登入 / key 失效）→ 要登入或填 key
//         connection refused    → sidecar 沒起（proxy 沒跑）
//      這兩類必須分清楚，別當成「模型沒回應」。
//
// ⚠ 與 janet 版的一個差異：C++ binding 是「編譯期連結 libcllm.so」，不像 janet 是執行期
//    (import llm)。所以沒有 :no-binding 這一類——編得過＝binding 就緒。

#include <cllm/llm.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace app {

// 失敗類別。對齊 janet 版的 :auth / :sidecar / :other（C++ 無 :no-binding，見檔頭）。
enum class Kind { Ok, Auth, Sidecar, Other };

inline const char *kind_name(Kind k) {
  switch (k) {
    case Kind::Ok: return "ok";
    case Kind::Auth: return "auth";
    case Kind::Sidecar: return "sidecar";
    case Kind::Other: return "other";
  }
  return "other";
}

// 退出碼帶語意（對齊 janet：sidecar=3，其餘一般錯誤=1）。
inline int exit_code_for(Kind k) {
  switch (k) {
    case Kind::Ok: return 0;
    case Kind::Sidecar: return 3;
    case Kind::Auth:
    case Kind::Other:
    default: return 1;
  }
}

inline std::string to_lower(std::string_view sv) {
  std::string s(sv);
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

inline bool contains(const std::string &hay, std::string_view needle) {
  return hay.find(needle) != std::string::npos;
}

// 把 binding 的錯誤訊息分成 :auth / :sidecar / :other，回 {類別, 人話說明}。
// 純字串邏輯、不觸網、不需憑證——這正是離線單元測試驗的東西。
struct Triage {
  Kind kind;
  std::string explain;
};

inline Triage classify_error(std::string_view raw) {
  const std::string m = to_lower(raw);
  const std::string orig(raw);

  if (contains(m, "401") || contains(m, "authentication") ||
      contains(m, "unauthorized") || contains(m, "authorization") ||
      contains(orig, "未登入") || contains(orig, "token 失效")) {
    return {Kind::Auth,
            "憑證問題（HTTP 401 / 未授權）：沒登入或 api_key 失效。\n"
            "    → Anthropic 直連：確認 config 的 api_key 是有效的 sk-ant-... 金鑰。\n"
            "    → 若走 OpenRouter OAuth：跑 `llm-login login` 重新帳號登入。"};
  }

  if (contains(m, "connection refused") || contains(m, "couldn't connect") ||
      contains(m, "could not connect") || contains(m, "connection reset") ||
      contains(m, "failed to connect")) {
    return {Kind::Sidecar,
            "連線被拒（connection refused）：anthropic-proxy sidecar 沒在跑。\n"
            "    → 先起 proxy：scripts/up.sh 會自動起（health-check 通過才跑 app）。"};
  }

  return {Kind::Other, std::string("其他錯誤：") + (orig.empty() ? "unknown" : orig)};
}

// ── 對外主呼叫 ────────────────────────────────────────────────
// 一次發問。成功回 {Kind::Ok, text}；失敗回 {分類, 說明}。
struct AskResult {
  Kind kind;
  std::string text;    // Ok 時是答案
  std::string explain; // 非 Ok 時是分流說明
};

// opts 直接透傳 llm::AskOpts（stream / on_delta）。
inline AskResult ask(const llm::Client &client, std::string_view prompt,
                     const llm::AskOpts &opts = {}) {
  llm::Result<std::string> r = client.ask(prompt, opts);
  if (r) return {Kind::Ok, *r, {}};
  Triage t = classify_error(r.error().message);
  return {t.kind, {}, t.explain};
}

} // namespace app
