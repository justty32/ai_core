// login_cli.cpp — `llm-login` CLI（消費 liblogin 的 C++ 入口，仿 cllm 的 llm CLI 樣式）。
//
//   llm-login login      瀏覽器登入一次 → 拿 token、patch cllm config
//   llm-login refresh     用 refresh_token 換新 access_token
//   llm-login token       印當前有效 token（快過期自動 refresh）——給腳本 $(...)
//   llm-login status      看目前狀態（不外洩 token 本身）
//   旗標：--provider P / --token T / --config C（覆寫預設路徑）

#include <ctime>
#include <iostream>
#include <string>

#include "login.hpp"
#include "store.hpp"

namespace {

int cmd_status(const llm::login::Paths &p) {
  login::store::J rec;
  try {
    rec = login::store::read_json(p.token);
  } catch (...) {
    std::cerr << "狀態：未登入。\n";
    return 1;
  }
  std::string at = rec.is_object() && rec.contains("access_token") && rec["access_token"].is_string()
                       ? rec["access_token"].get_string()
                       : "";
  std::string tail = at.size() >= 6 ? at.substr(at.size() - 6) : "??????";
  long now = static_cast<long>(std::time(nullptr));
  std::string note;
  if (!(rec.is_object() && rec.contains("expires_at"))) {
    note = "不過期型 token。";
  } else if (login::store::is_expired(rec, now)) {
    note = "已過期，下次用會自動 refresh。";
  } else {
    long left = static_cast<long>(rec["expires_at"].get_number()) - now;
    note = "剩約 " + std::to_string(left / 60) + " 分鐘到期。";
  }
  bool has_rt = rec.is_object() && rec.contains("refresh_token");
  std::cerr << "狀態：已登入（access_token …" << tail << "）。" << note
            << (has_rt ? "" : "（無 refresh_token）") << "\n";
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  std::string cmd, provider, token, config;
  bool open_browser = true;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if (a == "--provider") provider = next();
    else if (a == "--token") token = next();
    else if (a == "--config") config = next();
    else if (a == "--no-browser") open_browser = false;  // 只印網址、不自動開（headless／測試）
    else if (a == "-h" || a == "--help") { cmd = "help"; }
    else if (cmd.empty()) cmd = a;
  }

  auto paths = llm::login::resolve(provider.empty() ? nullptr : provider.c_str(),
                                   token.empty() ? nullptr : token.c_str(),
                                   config.empty() ? nullptr : config.c_str());

  auto usage = [] {
    std::cerr << "用法: llm-login {login|refresh|token|status} "
                 "[--provider P] [--token T] [--config C]\n";
  };

  try {
    if (cmd == "login") {
      std::string cfg = llm::login::do_login(paths, open_browser);
      std::cerr << "登入成功，憑證已寫進 " << cfg << "。\n";
      return 0;
    }
    if (cmd == "refresh") {
      std::string cfg = llm::login::do_refresh(paths);
      std::cerr << "已 refresh，憑證更新到 " << cfg << "。\n";
      return 0;
    }
    if (cmd == "token") {
      std::cout << llm::login::get_token(paths) << "\n";  // stdout 只有 token，方便 $(...)
      return 0;
    }
    if (cmd == "status") return cmd_status(paths);
    usage();
    return cmd == "help" ? 0 : 2;
  } catch (const llm::login::NeedLogin &) {
    std::cerr << "還沒登入（或憑證過期且無法 refresh）——先跑 login。\n";
    return 1;
  } catch (const std::exception &e) {
    std::cerr << "錯誤：" << e.what() << "\n";
    return 1;
  }
}
