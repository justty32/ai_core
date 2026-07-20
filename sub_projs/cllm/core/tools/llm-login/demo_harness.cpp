// demo_harness.cpp — cllm × llm-login 聯動流程的可跑 demo（免瀏覽器、假上游）。
//
// 展示設計中的「自動登入聯動」：**cllm 本身零改動**，聯動邏輯全在外層 harness——
//
//   cllm 呼叫 → 401（token 失效/沒登入）→ harness 攔到 → llm_login_login(&opts)
//      → 換 token → patch cllm config.json → harness 重試 cllm → 成功
//
// 沿用 llm-login 既有測試的 mock 手法（用 common/httpd 的微型 server 當假上游），把
// 真 OAuth／真後端（屬 WAIT_USER，要帳號＋瀏覽器）換成本機假伺服器，證明串接邏輯正確。
//
// 假上游有兩支（都跑在背景 thread）：
//   ① 假 LLM 後端：檢查 Authorization——沒帶對 token 回 401（附 OpenAI 風格 error 外殼），
//      帶對 "Bearer GOOD-TOKEN" 回 200 chat completion。＝cllm 的 401 從哪來。
//   ② 假 OAuth token 端：openrouter flow，換 code → 回 {"key":"GOOD-TOKEN"}。
// 「瀏覽器」也用假的：另起 thread 直接 GET llm-login 開的本機 callback，餵一個授權碼進去
//   （open_browser=0，login 只印網址不真開），把整條登入鏈跑通。
//
// cllm 端「真的」跑：本 harness 以子行程 exec 出貨的 `llm` CLI（argv[1] 指路，預設 ./build/llm），
// 只透過 LLM_CLI_CONFIG 指向暫存 config、看它的 exit code＋stderr——cllm 一行不動。
//
// 跑法：build/tools/llm-login-demo-harness [path/to/llm]
// 綠燈判準：401 被攔（exit=2＋stderr 見「HTTP 401」）→ login 被呼→config 被 patch→重試 exit=0。

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <glaze/glaze.hpp>

#include "http.hpp"       // cllm 出站笨管子（拿來當假「瀏覽器」打 callback）
#include "httpd.hpp"      // 共用微型 server（當假上游）
#include "login_abi.h"    // 聯動入口：llm_login_login（C-ABI）

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace {

// ── demo 常數 ──
const char *kGoodToken = "GOOD-TOKEN";     // 假 OAuth 換回、假 LLM 認得的 token
const char *kStaleToken = "STALE-TOKEN";   // 初始 config 帶的失效 token → 觸發 401
const int kLlmPort = 19710;                // 假 LLM 後端
const int kOAuthPort = 19711;              // 假 OAuth token 端
const int kRedirectPort = 19712;           // llm-login 開的 callback 埠

int g_fail = 0;
void expect(const char *what, bool ok) {
  std::printf("    [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++g_fail;
}

std::string read_file(const std::string &p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
void write_file(const std::string &p, const std::string &s) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f << s;
}
bool contains(const std::string &hay, const char *needle) {
  return hay.find(needle) != std::string::npos;
}

// 假 LLM 後端：驗 Authorization。帶對 token → 200 chat completion；否則 → 401＋error 外殼。
void serve_fake_llm(httpd::Server &srv) {
  srv.serve_forever([](const httpd::Request &req, httpd::Responder &res) {
    std::string auth = req.header("Authorization");
    if (auth == std::string("Bearer ") + kGoodToken) {
      res.send_json(200,
                    R"({"choices":[{"message":{"role":"assistant",)"
                    R"("content":"哈囉，我是假上游回覆（token 驗過了）"}}]})");
    } else {
      res.send_json(401,
                    R"({"error":{"message":"Unauthorized：token 失效或未登入",)"
                    R"("type":"authentication_error"}})");
    }
  });
}

// 假 OAuth token 端（openrouter flow）：換 code → 回不過期的 key。
void serve_fake_oauth(httpd::Server &srv) {
  srv.serve_forever([](const httpd::Request &req, httpd::Responder &res) {
    // openrouter flow：POST token_url，JSON body 帶 code；我們不校驗，直接回 key。
    (void)req;
    res.send_json(200, std::string("{\"key\":\"") + kGoodToken + "\"}");
  });
}

// 假「瀏覽器」：重試 GET llm-login 開的 callback，餵一個授權碼進去。
// login 的 capture_code 是 serve_once（阻塞 accept），故這裡要重試到它 listen 起來。
void fake_browser_hit_callback() {
  std::string url = "http://127.0.0.1:" + std::to_string(kRedirectPort) +
                    "/callback?code=FAKE-AUTH-CODE&state=";
  for (int i = 0; i < 100; ++i) {  // 最多重試 ~5s
    try {
      http::Request r;
      r.url = url;
      r.method = "GET";
      r.timeout_ms = 2000;
      http::Response resp = http::request(r);
      if (resp.status == 200) return;  // callback 收下了
    } catch (...) {
      // server 還沒起／連不上 → 稍等再試
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

// 跑出貨的 llm CLI：LLM_CLI_CONFIG 指向暫存 config，收 exit code＋stdout＋stderr。
struct RunResult {
  int exit_code = -1;
  std::string out, err;
};
RunResult run_cllm(const std::string &llm_bin, const std::string &cfg_path) {
  std::string out_path = "/tmp/llm_demo_stdout.txt";
  std::string err_path = "/tmp/llm_demo_stderr.txt";
  std::string cmd = "LLM_CLI_CONFIG='" + cfg_path + "' '" + llm_bin + "' ping >'" + out_path +
                    "' 2>'" + err_path + "'";
  int rc = std::system(cmd.c_str());
  RunResult r;
#ifndef _WIN32
  r.exit_code = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
#else
  r.exit_code = rc;
#endif
  r.out = read_file(out_path);
  r.err = read_file(err_path);
  return r;
}

}  // namespace

int main(int argc, char **argv) {
  std::string llm_bin = argc > 1 ? argv[1] : "./build/llm";

  // 暫存三檔（不碰使用者 ~/.config/llm）。
  std::string dir = "/tmp/llm_demo";
  std::system(("mkdir -p '" + dir + "'").c_str());
  std::string cfg_path = dir + "/config.json";
  std::string oauth_path = dir + "/oauth.json";
  std::string token_path = dir + "/oauth_token.json";
  std::remove(token_path.c_str());

  std::string llm_endpoint =
      "http://127.0.0.1:" + std::to_string(kLlmPort) + "/v1/chat/completions";

  // 初始 config：帶失效 token、指向假 LLM 後端 → 第一次呼叫必 401。
  write_file(cfg_path, std::string("{\"endpoint\":\"") + llm_endpoint + "\",\"api_key\":\"" +
                           kStaleToken + "\"}");

  // provider 設定（openrouter flow，指向假 OAuth；cllm_endpoint 指回同一個假 LLM）。
  {
    std::string oauth = "{";
    oauth += "\"flow\":\"openrouter\",";
    oauth += "\"authorize_url\":\"http://127.0.0.1:" + std::to_string(kOAuthPort) + "/auth\",";
    oauth += "\"token_url\":\"http://127.0.0.1:" + std::to_string(kOAuthPort) +
             "/api/v1/auth/keys\",";
    oauth += "\"redirect_host\":\"127.0.0.1\",";
    oauth += "\"redirect_port\":" + std::to_string(kRedirectPort) + ",";
    oauth += "\"redirect_path\":\"/callback\",";
    oauth += "\"cllm_endpoint\":\"" + llm_endpoint + "\",";
    oauth += "\"cllm_model\":\"mock-model\"";
    oauth += "}";
    write_file(oauth_path, oauth);
  }

  // 假上游：兩支 server 跑背景 thread。
  httpd::Server llm_srv("127.0.0.1", kLlmPort);
  httpd::Server oauth_srv("127.0.0.1", kOAuthPort);
  std::thread(serve_fake_llm, std::ref(llm_srv)).detach();
  std::thread(serve_fake_oauth, std::ref(oauth_srv)).detach();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 等 server 起

  std::printf("cllm × llm-login 聯動 demo（假上游、免瀏覽器）\n");
  std::printf("  llm 二進位：%s\n", llm_bin.c_str());
  std::printf("  假 LLM 後端：%s\n\n", llm_endpoint.c_str());

  // ── 步驟 ① 呼 cllm（帶失效 token）→ 應撞 401 ──
  std::printf("① 呼 cllm（config 帶失效 token）……\n");
  RunResult r1 = run_cllm(llm_bin, cfg_path);
  std::printf("    cllm exit=%d, stderr=%s", r1.exit_code,
              r1.err.empty() ? "(空)\n" : r1.err.c_str());
  bool got_401 = (r1.exit_code == 2) && contains(r1.err, "HTTP 401");
  expect("cllm 撞 401（exit=2 且 stderr 見 HTTP 401）", got_401);

  // ── 步驟 ② harness 攔到 401 → 觸發 llm_login_login ──
  std::printf("\n② harness 攔到 401 → 呼 llm_login_login（開瀏覽器換 token→patch config）……\n");
  bool login_triggered = got_401;  // 就是「攔到 401 才登入」這個決策
  expect("harness 依 401 決定觸發登入", login_triggered);

  llm_login_status_t login_st = LLM_LOGIN_ERR;
  if (login_triggered) {
    // 假瀏覽器（另一 thread）餵授權碼給 login 開的 callback。
    std::thread browser(fake_browser_hit_callback);

    char errbuf[512] = {0};
    llm_login_opts_t opts{};
    opts.provider_path = oauth_path.c_str();
    opts.token_path = token_path.c_str();
    opts.config_path = cfg_path.c_str();
    opts.open_browser = 0;  // 不真開瀏覽器；假瀏覽器 thread 代打 callback
    opts.err = errbuf;
    opts.err_len = sizeof(errbuf);

    login_st = llm_login_login(&opts);
    browser.join();
    std::printf("    llm_login_login → status=%d %s\n", login_st,
                login_st == LLM_LOGIN_OK ? "(OK)" : errbuf);
  }
  expect("llm_login_login 回 OK", login_st == LLM_LOGIN_OK);

  // ── 步驟 ③ 驗 config 被 patch 成新 token ──
  std::printf("\n③ 檢查 cllm config 被 patch……\n");
  std::string patched = read_file(cfg_path);
  std::printf("    config.json = %s\n", patched.c_str());
  expect("config 的 api_key 換成新 token", contains(patched, kGoodToken));
  expect("config 不再帶失效 token", !contains(patched, kStaleToken));

  // ── 步驟 ④ harness 重試 cllm → 應成功 ──
  std::printf("\n④ harness 重試 cllm……\n");
  RunResult r2 = run_cllm(llm_bin, cfg_path);
  std::printf("    cllm exit=%d, stdout=%s", r2.exit_code,
              r2.out.empty() ? "(空)\n" : r2.out.c_str());
  expect("cllm 重試成功（exit=0）", r2.exit_code == 0);
  expect("cllm 收到假上游回覆", contains(r2.out, "token 驗過了"));

  std::printf("\n%s\n", g_fail == 0 ? "聯動全綠 ✅（401→攔→login→patch→重試成功）"
                                    : "有紅 ❌");
  return g_fail == 0 ? 0 : 1;
}
