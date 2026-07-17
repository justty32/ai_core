// test_offline.cpp — llm-login 離線煙霧測試（不連網、不開瀏覽器、不起 server）。
//
// 測確定性的部分：crypto（SHA-256 已知答案、base64url、PKCE 自洽）、authorize URL 組裝、
// config patch 保留其餘鍵、token 記錄 expires_at 計算。跑：build/tools/llm-login-test-offline。

#include <cstdio>
#include <string>

#include "crypto.hpp"
#include "oauth.hpp"
#include "store.hpp"

static int g_fail = 0;
static void check(const char *name, bool ok) {
  std::printf("  %s %s\n", ok ? " ok " : "FAIL", name);
  if (!ok) ++g_fail;
}

static std::string hex(const std::vector<uint8_t> &b) {
  static const char *h = "0123456789abcdef";
  std::string s;
  for (uint8_t c : b) {
    s.push_back(h[c >> 4]);
    s.push_back(h[c & 15]);
  }
  return s;
}

static void test_sha256() {
  std::printf("• sha256\n");
  // NIST 已知答案
  check("sha256(\"abc\")",
        hex(login::crypto::sha256("abc")) ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  check("sha256(\"\")",
        hex(login::crypto::sha256("")) ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

static void test_base64url() {
  std::printf("• base64url\n");
  auto b = [](std::string s) { return std::vector<uint8_t>(s.begin(), s.end()); };
  check("\"Man\"→TWFu", login::crypto::base64url_nopad(b("Man")) == "TWFu");
  check("\"Ma\"→TWE（無 padding）", login::crypto::base64url_nopad(b("Ma")) == "TWE");
  check("\"M\"→TQ（無 padding）", login::crypto::base64url_nopad(b("M")) == "TQ");
}

static void test_pkce() {
  std::printf("• pkce\n");
  auto [verifier, challenge] = login::crypto::gen_pkce();
  check("verifier 43 字元（base64url of 32B）", verifier.size() == 43);
  check("challenge==base64url(sha256(verifier))",
        challenge == login::crypto::base64url_nopad(login::crypto::sha256(verifier)));
  check("challenge 43 字元", challenge.size() == 43);
  auto [v2, c2] = login::crypto::gen_pkce();
  check("兩次不同（隨機）", verifier != v2);
}

static void test_authorize_url() {
  std::printf("• authorize_url\n");
  login::oauth::J prov;
  glz::read_json(prov, R"({"authorize_url":"https://auth.example/authorize",
    "client_id":"cid123","scope":"read write","token_url":"https://auth.example/token"})");
  std::string url = login::oauth::authorize_url(prov, "http://127.0.0.1:8123/callback", "st8",
                                                "chal");
  auto has = [&](const char *s) { return url.find(s) != std::string::npos; };
  check("response_type=code", has("response_type=code"));
  check("client_id 帶入", has("client_id=cid123"));
  check("redirect_uri URL 編碼", has("redirect_uri=http%3A%2F%2F127.0.0.1%3A8123%2Fcallback"));
  check("scope 編碼（空白→%20）", has("scope=read%20write"));
  check("state 帶入", has("state=st8"));
  check("S256", has("code_challenge_method=S256"));

  login::oauth::Redirect rd = login::oauth::redirect_of(prov);
  check("redirect 預設 127.0.0.1:8123/callback",
        rd.host == "127.0.0.1" && rd.port == 8123 && rd.path == "/callback");
}

static void test_config_patch() {
  std::printf("• config patch\n");
  std::string tmp = "/tmp/llm_login_test_cfg.json";
  login::store::write_file(tmp, R"({"api_key":"old","timeout_ms":5000})", false);
  login::store::patch_cllm(tmp, "sk-new", "http://x/y", "m1");
  login::store::J back = login::store::read_json(tmp);
  auto s = [&](const char *k) {
    return back.contains(k) && back[k].is_string() ? back[k].get_string() : std::string();
  };
  check("api_key 被換", s("api_key") == "sk-new");
  check("endpoint 寫入", s("endpoint") == "http://x/y");
  check("model 寫入", s("model") == "m1");
  check("其餘鍵原樣保留（timeout_ms）",
        back.contains("timeout_ms") && back["timeout_ms"].is_number() &&
            back["timeout_ms"].get_number() == 5000);
  std::remove(tmp.c_str());
}

static void test_token_record() {
  std::printf("• token record\n");
  login::store::J raw;
  glz::read_json(raw, R"({"access_token":"a1","refresh_token":"r1","expires_in":3600})");
  login::store::J rec = login::store::make_token_record(raw, 1000000);
  check("expires_at = now+expires_in",
        rec.contains("expires_at") && rec["expires_at"].get_number() == 1000000 + 3600);
  check("未過期（now）", !login::store::is_expired(rec, 1000000));
  check("已過期（now 超過）", login::store::is_expired(rec, 1000000 + 4000));
  login::store::J norefresh;
  glz::read_json(norefresh, R"({"access_token":"k1"})");
  login::store::J rec2 = login::store::make_token_record(norefresh, 1000000);
  check("無 expires_in → 不過期型永不過期", !login::store::is_expired(rec2, 9999999999));
}

int main() {
  test_sha256();
  test_base64url();
  test_pkce();
  test_authorize_url();
  test_config_patch();
  test_token_record();
  std::printf("\n%s\n", g_fail == 0 ? "全綠 ✅" : "有紅 ❌");
  return g_fail == 0 ? 0 : 1;
}
