// main.cpp —— cpp-try-1 CLI 進入點：讀 endpoint/api-key/model → 呼 app::ask → 印結果 / 分流錯誤。
//
// 用 C++23 + cllm C++ binding（<cllm/llm.hpp> 的 llm::Client::ask、llm_reflect.hpp 的 ask_as<T>）。
//
// 跑法（通常經 scripts/up.sh，它會先備好 proxy＋憑證再帶環境變數進來）：
//   ./build/cpp-try-1 用一句話介紹你自己
//   ./build/cpp-try-1 --stream 寫一首短詩
//   ./build/cpp-try-1 --json  給我一個關於轉發代理的冷知識     # 結構化輸出（ask_as<T>）
//   ./build/cpp-try-1 --check                                  # 離線自檢：環境變數齊？失敗分流對不對？
//   APP_ENDPOINT=... APP_API_KEY=... ./build/cpp-try-1 你好
//
// 環境變數（scripts/up.sh 會設；也可自己給）：
//   APP_ENDPOINT  cllm endpoint（預設本機 proxy）
//   APP_API_KEY   Bearer token（sk-ant-... 或 OAuth 換來的 key）
//   APP_MODEL     模型 id（預設 claude-opus-4-8）
#include "app.hpp"

#include <cllm/llm_reflect.hpp> // ask_as<T>：struct 進 struct 出（反射生 schema）

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char *kDefaultEndpoint = "http://127.0.0.1:8787/v1/chat/completions";
constexpr const char *kDefaultModel = "claude-opus-4-8";

std::string env_or(const char *k, std::string dflt) {
  const char *v = std::getenv(k);
  return (v && *v) ? std::string(v) : std::move(dflt);
}

// ── 結構化輸出的目標型別（給 --json；ask_as<T> 用反射生 JSON schema，回應直接解回這個）──
// ⚠ glaze 反射要求 struct 放檔案層級（namespace scope），不能是函式內 local。
struct Fact {
  std::string title; // 冷知識標題
  std::string body;  // 內文
};

// 離線自檢：不觸網，回報環境狀態＋失敗分流自測。對齊 janet 版 do-check。
int do_check(const std::string &endpoint, const std::string &model, bool have_key) {
  std::puts("cpp-try-1 自檢");
  std::puts("  cllm C++ binding：已連結（能編出這支＝<cllm/llm.hpp> 就緒；非執行期載入）");
  std::printf("  APP_ENDPOINT：%s\n", endpoint.c_str());
  std::printf("  APP_MODEL：   %s\n", model.c_str());
  std::printf("  APP_API_KEY： %s\n", have_key ? "已設（不外洩內容）" : "未設 → 會撞 401");
  std::puts("  失敗分流自測：");

  struct Row { const char *msg; app::Kind expect; };
  const Row rows[] = {
      {"後端錯誤 (HTTP 401): Unauthorized", app::Kind::Auth},
      {"curl: (7) Failed to connect: Connection refused", app::Kind::Sidecar},
      {"something else broke", app::Kind::Other},
  };
  bool all_ok = true;
  for (const Row &r : rows) {
    app::Triage t = app::classify_error(r.msg);
    bool ok = (t.kind == r.expect);
    all_ok = all_ok && ok;
    std::printf("    %-45s → %-8s %s\n", r.msg, app::kind_name(t.kind), ok ? "OK" : "MISMATCH");
  }
  return all_ok ? 0 : 1;
}

void usage() {
  std::fputs(
      "cpp-try-1 —— 用 C++ 打 Anthropic API（經 cllm C++ binding → anthropic-proxy）\n"
      "用法： cpp-try-1 [--stream|-s] [--json] [--check] [--endpoint URL] [--model ID] <prompt...>\n"
      "  --stream/-s   串流輸出（逐段印）\n"
      "  --json        結構化輸出（ask_as<Fact>，struct 進 struct 出）\n"
      "  --check       離線自檢（不觸網），看環境／失敗分流是否備妥\n"
      "  --endpoint    覆寫 endpoint（預設 $APP_ENDPOINT 或本機 proxy）\n"
      "  --model/-m    覆寫模型 id\n",
      stderr);
}

} // namespace

int main(int argc, char **argv) {
  bool stream = false, want_json = false, want_check = false;
  std::string opt_endpoint, opt_model;
  std::vector<std::string> words;

  for (int i = 1; i < argc; ++i) {
    std::string_view a = argv[i];
    if (a == "--stream" || a == "-s") stream = true;
    else if (a == "--json") want_json = true;
    else if (a == "--check") want_check = true;
    else if ((a == "--endpoint") && i + 1 < argc) opt_endpoint = argv[++i];
    else if ((a == "--model" || a == "-m") && i + 1 < argc) opt_model = argv[++i];
    else if (a == "--help" || a == "-h") { usage(); return 0; }
    else words.emplace_back(a);
  }

  const std::string endpoint =
      !opt_endpoint.empty() ? opt_endpoint : env_or("APP_ENDPOINT", kDefaultEndpoint);
  const std::string model = !opt_model.empty() ? opt_model : env_or("APP_MODEL", kDefaultModel);
  const char *key_env = std::getenv("APP_API_KEY");
  const bool have_key = key_env && *key_env;

  if (want_check) return do_check(endpoint, model, have_key);

  std::string prompt;
  for (size_t i = 0; i < words.size(); ++i) {
    if (i) prompt += ' ';
    prompt += words[i];
  }
  if (prompt.empty()) {
    std::fputs("沒給 prompt。範例：cpp-try-1 你好   （或 --check 自檢）\n", stderr);
    return 2;
  }

  // 佈好 client：endpoint / api_key / model 三個值餵給 binding（INTEGRATION.md 情境 A）。
  llm::Client client;
  client.endpoint = endpoint;
  if (have_key) client.api_key = key_env;
  client.model = model;

  std::printf("· endpoint=%s model=%s api_key=%s\n", endpoint.c_str(), model.c_str(),
              have_key ? "已設" : "未設");

  // ── 結構化輸出路線：ask_as<Fact> —— struct 即唯一真相源，回應解回 struct ──
  if (want_json) {
    llm::Result<Fact> r = llm::ask_as<Fact>(client, prompt);
    if (r) {
      std::printf("title: %s\nbody:  %s\n", r->title.c_str(), r->body.c_str());
      return 0;
    }
    app::Triage t = app::classify_error(r.error().message);
    std::fprintf(stderr, "✗ 失敗（%s）：\n    %s\n", app::kind_name(t.kind), t.explain.c_str());
    return app::exit_code_for(t.kind);
  }

  // ── 一般路線：ask（純文字，串流可選）──
  llm::AskOpts opts;
  opts.stream = stream;
  if (stream)
    opts.on_delta = [](std::string_view sv) {
      std::fwrite(sv.data(), 1, sv.size(), stdout);
      std::fflush(stdout);
      return false; // 不主動中止
    };

  app::AskResult res = app::ask(client, prompt, opts);
  if (res.kind == app::Kind::Ok) {
    if (stream) std::putchar('\n'); // 串流時補換行收尾
    else std::printf("%s\n", res.text.c_str());
    return 0;
  }
  std::fprintf(stderr, "✗ 失敗（%s）：\n    %s\n", app::kind_name(res.kind), res.explain.c_str());
  return app::exit_code_for(res.kind);
}
