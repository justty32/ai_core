// login.cpp — llm-login 的 C++ 編排 ＋ C-ABI 出口 ＋ 開瀏覽器。
//
// 三流程用 oauth＋store＋crypto 拼；C-ABI（login_abi.h）在末尾 try/catch 這些函數填 err。

#include "login.hpp"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>

#include "crypto.hpp"
#include "login_abi.h"
#include "oauth.hpp"
#include "store.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace llm::login {
// 低層模組在頂層 namespace `login`（login::store 等）；本檔在 `llm::login` 內，
// 直寫 store:: 會被解成 llm::login::store（不存在），故建別名指回頂層。
namespace store = ::login::store;
namespace oauth = ::login::oauth;
namespace crypto = ::login::crypto;

namespace login_impl {  // 具名子 namespace（對齊專案慣例）

using store::J;

std::string sget(J &j, const char *k, std::string d = "") {
  return j.is_object() && j.contains(k) && j[k].is_string() ? j[k].get_string() : d;
}

J read_token_or_empty(const std::string &path) {
  try {
    return store::read_json(path);
  } catch (...) {
    J e;
    e = J::object_t{};
    return e;
  }
}

std::string dump(J &j) {
  std::string s;
  (void)glz::write_json(j, s);
  return s;
}

void announce_and_open(const std::string &url, bool open) {
  std::cerr << "開瀏覽器登入；若沒自動彈出，手動開這個網址：\n  " << url << "\n";
  if (open) open_browser(url);
}

// refresh 回應常不帶新 refresh_token → 保留舊的。
void keep_refresh(J &newrec, const std::string &old_rt) {
  if (!old_rt.empty() && !(newrec.is_object() && newrec.contains("refresh_token")))
    newrec["refresh_token"] = old_rt;
}

}  // namespace login_impl
using namespace login_impl;

Paths resolve(const char *pr, const char *tk, const char *cf) {
  auto pick = [](const char *val, const char *env, const std::string &def) -> std::string {
    if (val && *val) return val;
    const char *e = env ? std::getenv(env) : nullptr;
    if (e && *e) return e;
    return def;
  };
  Paths p;
  p.provider = pick(pr, "LLM_OAUTH_PROVIDER", store::default_path("oauth.json"));
  p.token = pick(tk, nullptr, store::default_path("oauth_token.json"));
  p.config = pick(cf, "LLM_CLI_CONFIG", store::default_path("config.json"));
  return p;
}

void open_browser(const std::string &url) {
#ifdef _WIN32
  ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
  pid_t pid = ::fork();
  if (pid == 0) {  // child：依序試 xdg-open／open
    ::execlp("xdg-open", "xdg-open", url.c_str(), static_cast<char *>(nullptr));
    ::execlp("open", "open", url.c_str(), static_cast<char *>(nullptr));
    ::_exit(127);
  }
  // parent：不等（讓瀏覽器留著）
#endif
}

std::string do_login(const Paths &p, bool open) {
  J prov = store::read_json(p.provider);
  oauth::Redirect rd = oauth::redirect_of(prov);
  auto [verifier, challenge] = crypto::gen_pkce();
  long now = static_cast<long>(std::time(nullptr));
  J token_rec;

  if (sget(prov, "flow") == "openrouter") {
    std::string url = oauth::authorize_url_openrouter(prov, rd.uri, challenge);
    announce_and_open(url, open);
    std::string code = oauth::capture_code(rd.host, rd.port, rd.path, "");
    J raw = oauth::exchange_code_openrouter(prov, code, verifier);
    std::string key = sget(raw, "key");
    if (key.empty()) key = sget(raw, "access_token");
    if (key.empty()) throw std::runtime_error("交換回應沒有 key/access_token");
    token_rec = J::object_t{};
    token_rec["access_token"] = key;  // 不過期、無 refresh
  } else {
    std::string state = crypto::base64url_nopad(crypto::random_bytes(16));
    std::string url = oauth::authorize_url(prov, rd.uri, state, challenge);
    announce_and_open(url, open);
    std::string code = oauth::capture_code(rd.host, rd.port, rd.path, state);
    J raw = oauth::exchange_code(prov, code, rd.uri, verifier);
    token_rec = store::make_token_record(raw, now);
  }

  std::string access = sget(token_rec, "access_token");
  if (access.empty()) throw std::runtime_error("沒拿到 access_token");
  store::write_file(p.token, dump(token_rec), true);
  return store::patch_cllm(p.config, access, sget(prov, "cllm_endpoint"),
                           sget(prov, "cllm_model"));
}

std::string do_refresh(const Paths &p) {
  J prov = store::read_json(p.provider);
  J rec = read_token_or_empty(p.token);
  std::string rt = sget(rec, "refresh_token");
  if (rt.empty()) throw NeedLogin();
  long now = static_cast<long>(std::time(nullptr));
  J raw = oauth::refresh(prov, rt);
  J newrec = store::make_token_record(raw, now);
  keep_refresh(newrec, rt);
  store::write_file(p.token, dump(newrec), true);
  return store::patch_cllm(p.config, sget(newrec, "access_token"),
                           sget(prov, "cllm_endpoint"), sget(prov, "cllm_model"));
}

std::string get_token(const Paths &p) {
  J rec = read_token_or_empty(p.token);
  if (!rec.is_object() || !rec.contains("access_token")) throw NeedLogin();
  long now = static_cast<long>(std::time(nullptr));
  if (store::is_expired(rec, now)) {
    std::string rt = sget(rec, "refresh_token");
    if (rt.empty()) throw NeedLogin();
    J prov = store::read_json(p.provider);
    J raw = oauth::refresh(prov, rt);
    J newrec = store::make_token_record(raw, now);
    keep_refresh(newrec, rt);
    store::write_file(p.token, dump(newrec), true);
    store::patch_cllm(p.config, sget(newrec, "access_token"), sget(prov, "cllm_endpoint"),
                      sget(prov, "cllm_model"));
    rec = std::move(newrec);
  }
  return sget(rec, "access_token");
}

}  // namespace llm::login

// ══════════════════════════ C ABI 出口 ══════════════════════════
namespace {
void set_err(const llm_login_opts_t *o, const std::string &m) {
  if (o && o->err && o->err_len) {
    std::strncpy(o->err, m.c_str(), o->err_len - 1);
    o->err[o->err_len - 1] = '\0';
  }
}
llm::login::Paths resolve_opts(const llm_login_opts_t *o) {
  return llm::login::resolve(o ? o->provider_path : nullptr, o ? o->token_path : nullptr,
                             o ? o->config_path : nullptr);
}
}  // namespace

extern "C" llm_login_status_t llm_login_login(const llm_login_opts_t *opts) {
  using namespace llm::login;
  try {
    do_login(resolve_opts(opts), opts && opts->open_browser);
    return LLM_LOGIN_OK;
  } catch (const NeedLogin &) {
    return LLM_LOGIN_NEED_LOGIN;
  } catch (const std::exception &e) {
    set_err(opts, e.what());
    return LLM_LOGIN_ERR;
  }
}

extern "C" llm_login_status_t llm_login_refresh(const llm_login_opts_t *opts) {
  using namespace llm::login;
  try {
    do_refresh(resolve_opts(opts));
    return LLM_LOGIN_OK;
  } catch (const NeedLogin &) {
    return LLM_LOGIN_NEED_LOGIN;
  } catch (const std::exception &e) {
    set_err(opts, e.what());
    return LLM_LOGIN_ERR;
  }
}

extern "C" long llm_login_token(const llm_login_opts_t *opts, char *buf, size_t buf_len) {
  using namespace llm::login;
  try {
    std::string t = get_token(resolve_opts(opts));
    if (t.size() + 1 > buf_len) return -2;
    std::memcpy(buf, t.data(), t.size());
    buf[t.size()] = '\0';
    return static_cast<long>(t.size());
  } catch (const NeedLogin &) {
    return -1;
  } catch (const std::exception &e) {
    set_err(opts, e.what());
    return -3;
  }
}
