// login.hpp — llm-login 的 C++ 入口（薄核心，C-ABI 在其上包）。
//
// 三條流程 do_login／do_refresh／get_token 用 oauth＋store＋crypto 拼起來；丟例外報錯。
// C-ABI（login_abi.h）在 login.cpp 內 try/catch 這些函數、填 opts->err。

#pragma once
#include <stdexcept>
#include <string>

namespace llm::login {

// 三個檔路徑（空＝呼叫端未指定，resolve 會補 env/預設）。
struct Paths {
  std::string provider;
  std::string token;
  std::string config;
};

// nullptr/"" → env（LLM_OAUTH_PROVIDER／LLM_CLI_CONFIG）或 ~/.config/llm/… 預設。
Paths resolve(const char *provider, const char *token, const char *config);

// 沒憑證/過期且無法 refresh → 丟這個（C-ABI 對映 LLM_LOGIN_NEED_LOGIN）。
struct NeedLogin : std::runtime_error {
  NeedLogin() : std::runtime_error("沒有效憑證且無法 refresh——請重跑 login") {}
};

// 跑完整登入流程（open_browser=true 時自動開瀏覽器）；成功回 patch 的 config 路徑。
std::string do_login(const Paths &, bool open_browser);
// refresh；成功回 config 路徑。無 refresh_token → 丟 NeedLogin。
std::string do_refresh(const Paths &);
// 取有效 token（快過期自動 refresh＋patch）。沒登入 → 丟 NeedLogin。
std::string get_token(const Paths &);

// 開系統瀏覽器到 url（POSIX xdg-open/open；Windows ShellExecute）。失敗只印提示、不丟。
void open_browser(const std::string &url);

}  // namespace llm::login
