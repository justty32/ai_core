// oauth.cpp — OAuth 2.0 授權碼＋PKCE 實作。出站重用 cllm 的 http（src/http）。

#include "oauth.hpp"

#include <stdexcept>

#include "http.hpp"    // cllm 出站笨管子
#include "httpd.hpp"   // 共用入站 server（接 callback）

namespace login::oauth {
namespace oauth_impl {  // 具名子 namespace（對齊專案慣例）

std::string sget(J &j, const char *k, std::string d = "") {
  return j.is_object() && j.contains(k) && j[k].is_string() ? j[k].get_string() : d;
}

int iget(J &j, const char *k, int d) {
  return j.is_object() && j.contains(k) && j[k].is_number()
             ? static_cast<int>(j[k].get_number())
             : d;
}

std::string url_encode(const std::string &s) {
  static const char *hex = "0123456789ABCDEF";
  std::string out;
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 15]);
    }
  }
  return out;
}

std::string append_param(std::string url, const std::string &k, const std::string &v) {
  url.push_back(url.find('?') == std::string::npos ? '?' : '&');
  url += url_encode(k) + "=" + url_encode(v);
  return url;
}

// POST 一包，解回應 JSON；後端錯誤（4xx 或含 error）→ throw 帶訊息。
J post_json_reply(const std::string &url, const std::vector<std::string> &headers,
                  const std::string &body) {
  http::Request req;
  req.url = url;
  req.method = "POST";
  req.headers = headers;
  req.body = body;
  http::Response resp = http::request(req);
  J j;
  bool ok = !glz::read_json(j, resp.body);
  if (resp.status >= 400 || (ok && j.is_object() && j.contains("error"))) {
    std::string msg = ok && j.is_object() && j.contains("error")
                          ? (j["error"].is_string() ? j["error"].get_string()
                                                    : resp.body.substr(0, 200))
                          : resp.body.substr(0, 200);
    throw std::runtime_error("token 端錯誤 (HTTP " + std::to_string(resp.status) + "): " + msg);
  }
  if (!ok) throw std::runtime_error("token 回應非 JSON：" + resp.body.substr(0, 200));
  return j;
}

}  // namespace oauth_impl
using namespace oauth_impl;

Redirect redirect_of(J &prov) {
  Redirect r;
  r.host = sget(prov, "redirect_host", "127.0.0.1");
  r.port = iget(prov, "redirect_port", 8123);
  r.path = sget(prov, "redirect_path", "/callback");
  r.uri = "http://" + r.host + ":" + std::to_string(r.port) + r.path;
  return r;
}

std::string authorize_url(J &prov, const std::string &redirect_uri, const std::string &state,
                          const std::string &challenge) {
  std::string url = sget(prov, "authorize_url");
  url = append_param(url, "response_type", "code");
  std::string cid = sget(prov, "client_id");
  if (!cid.empty()) url = append_param(url, "client_id", cid);
  url = append_param(url, "redirect_uri", redirect_uri);
  std::string scope = sget(prov, "scope");
  if (!scope.empty()) url = append_param(url, "scope", scope);
  if (!state.empty()) url = append_param(url, "state", state);
  url = append_param(url, "code_challenge", challenge);
  url = append_param(url, "code_challenge_method", "S256");
  return url;
}

std::string authorize_url_openrouter(J &prov, const std::string &redirect_uri,
                                     const std::string &challenge) {
  std::string url = sget(prov, "authorize_url", "https://openrouter.ai/auth");
  url = append_param(url, "callback_url", redirect_uri);
  url = append_param(url, "code_challenge", challenge);
  url = append_param(url, "code_challenge_method", "S256");
  return url;
}

std::string capture_code(const std::string &host, int port, const std::string &path,
                         const std::string &expected_state) {
  std::string code, err;
  httpd::Server srv(host, port);
  srv.serve_once([&](const httpd::Request &req, httpd::Responder &res) {
    if (req.path != path) {
      res.send(404, "text/plain", "not found");
      return;
    }
    std::string got_state = req.query_param("state");
    code = req.query_param("code");
    if (code.empty()) {
      err = "callback 沒帶 code（" + req.query + "）";
      res.send(400, "text/html", "<h3>登入失敗：沒拿到 code，可關閉本頁。</h3>");
      return;
    }
    if (!expected_state.empty() && got_state != expected_state) {
      err = "state 不符（可能 CSRF）";
      code.clear();
      res.send(400, "text/html", "<h3>登入失敗：state 不符，可關閉本頁。</h3>");
      return;
    }
    res.send(200, "text/html", "<h3>登入完成，可關閉本頁回終端機。</h3>");
  });
  if (!err.empty()) throw std::runtime_error(err);
  if (code.empty()) throw std::runtime_error("沒拿到授權碼");
  return code;
}

J exchange_code(J &prov, const std::string &code, const std::string &redirect_uri,
                const std::string &verifier) {
  std::string body = "grant_type=authorization_code";
  body += "&code=" + url_encode(code);
  body += "&redirect_uri=" + url_encode(redirect_uri);
  body += "&code_verifier=" + url_encode(verifier);
  std::string cid = sget(prov, "client_id");
  if (!cid.empty()) body += "&client_id=" + url_encode(cid);
  std::string secret = sget(prov, "client_secret");
  if (!secret.empty()) body += "&client_secret=" + url_encode(secret);
  return post_json_reply(sget(prov, "token_url"),
                         {"content-type: application/x-www-form-urlencoded"}, body);
}

J exchange_code_openrouter(J &prov, const std::string &code, const std::string &verifier) {
  std::string url = sget(prov, "token_url", "https://openrouter.ai/api/v1/auth/keys");
  std::string body = "{\"code\":\"" + code + "\",\"code_verifier\":\"" + verifier +
                     "\",\"code_challenge_method\":\"S256\"}";
  return post_json_reply(url, {"content-type: application/json"}, body);
}

J refresh(J &prov, const std::string &refresh_token) {
  std::string body = "grant_type=refresh_token";
  body += "&refresh_token=" + url_encode(refresh_token);
  std::string cid = sget(prov, "client_id");
  if (!cid.empty()) body += "&client_id=" + url_encode(cid);
  std::string secret = sget(prov, "client_secret");
  if (!secret.empty()) body += "&client_secret=" + url_encode(secret);
  return post_json_reply(sget(prov, "token_url"),
                         {"content-type: application/x-www-form-urlencoded"}, body);
}

}  // namespace login::oauth
