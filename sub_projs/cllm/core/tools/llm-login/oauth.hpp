// oauth.hpp — OAuth 2.0 授權碼＋PKCE 協定層（介面）。供應商中立、不硬編任何一家。
//
// 兩條路：標準 OAuth（授權碼＋PKCE S256、token 端 POST form）與 OpenRouter 變體
// （callback_url＋PKCE、token 端 POST JSON 換不過期 key）。出站重用 cllm 的 http。

#pragma once
#include <string>

#include <glaze/glaze.hpp>

namespace login::oauth {

using J = glz::json_t;

struct Redirect {
  std::string uri;   // http://host:port/path
  std::string host;
  int port;
  std::string path;
};
Redirect redirect_of(J &prov);

// 標準 authorize URL（response_type=code, PKCE S256）。
std::string authorize_url(J &prov, const std::string &redirect_uri, const std::string &state,
                          const std::string &challenge);
// OpenRouter 變體（callback_url＋challenge，無 state/client_id）。
std::string authorize_url_openrouter(J &prov, const std::string &redirect_uri,
                                     const std::string &challenge);

// 起本機 server 接 callback，回 code（expected_state 非空才比對；不符 throw）。阻塞至瀏覽器打回。
std::string capture_code(const std::string &host, int port, const std::string &path,
                         const std::string &expected_state);

// 標準：POST form 換 token。回 raw token JSON（access_token/expires_in/refresh_token…）。失敗 throw。
J exchange_code(J &prov, const std::string &code, const std::string &redirect_uri,
                const std::string &verifier);
// OpenRouter：POST JSON {code, code_verifier, code_challenge_method} 換 {key}。失敗 throw。
J exchange_code_openrouter(J &prov, const std::string &code, const std::string &verifier);
// refresh_token 換新 access_token。回 raw token JSON。失敗 throw。
J refresh(J &prov, const std::string &refresh_token);

}  // namespace login::oauth
