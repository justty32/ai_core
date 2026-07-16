// cli.cpp — cli::run 的 orchestrator：把命令列組成一次 llm::abi::Client::ask 的發問。
//
// 流程（各步驟的機制拆在旁邊的 TU）：
//   (1) 解析 argv：固定旗標（--stream/--image/--schema/--config/--help/--）特判；反射旗標
//       （cli_flags::client_flags 的合法表）吃下一個 argv 收進 raw_values；其餘當位置參數拼 prompt。
//   (2) 定 prompt：有位置參數就拼；否則且 stdin 非終端才整段讀（互動終端不讀，避免卡住）。
//   (3) 組 Client：內建預設 → config 檔（cli_config::load_into）→ 反射把 raw_values coerce 覆寫。
//   (4) 組 Request（prompt＋stream＋--schema 檔＋--image 檔）→ client.ask()，output 走 handlers。
//
// 反射旗標的機制（型別分派／--help）在 cli_flags.*；config/檔案在 cli_config.*；共用常數在 cli_internal.hpp。

#include "cli.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <io.h> // _isatty / _fileno
#else
#include <unistd.h> // isatty / fileno
#endif

#include <glaze/glaze.hpp>  // for_each_field / reflect（coerce 反射旗標進 Client）
#include "cabi.hpp"         // 薄鏡像：Client / Request / Handlers / Context / Status
#include "cli_config.hpp"   // read_file / mime_of / load_into
#include "cli_flags.hpp"    // client_flags / print_usage / assign_field / kebab_flag
#include "cli_internal.hpp" // kExit* 退出碼

namespace cli {
namespace {

// stdin 是否為互動終端。是＝別阻塞去讀（沒給 prompt 就直接報錯，不卡住）；
// 否（被導管/檔案餵入）＝照舊整段讀。
bool stdin_is_tty() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(fileno(stdin)) != 0;
#endif
}

// ── SIGINT → 取消在途請求（POSIX）。handler 只做 atomic 存（見 Context::cancel），async-signal 安全。
std::atomic<llm::abi::Context *> g_ctx{nullptr};
#ifndef _WIN32
extern "C" void on_sigint(int) {
  if (auto *c = g_ctx.load())
    c->cancel();
}
#endif

} // namespace

int run(const std::vector<std::string> &args) {
  // 合法反射旗標表：--flag → Client 欄位名
  std::map<std::string, std::string> flag_to_field;
  for (const auto &fi : flags::client_flags())
    flag_to_field[fi.flag] = fi.field;

  // ── (1) 解析 argv（args[0]＝執行檔，從 1 起）──
  std::map<std::string, std::string> raw_values; // 反射欄位名 → 原始字串
  std::vector<std::string> prompt_parts;         // 位置參數 → 拼成 prompt
  std::vector<std::string> image_paths;          // --image（可重複）
  std::string schema_path, config_path;
  bool has_schema = false, has_config = false, stream = false;
  bool no_more_flags = false; // 見到 "--" 後，其餘 argv 一律當 prompt（unix 慣例）

  // 需要吃下一個 argv 的取值小工具：缺值即報錯。
  auto need_value = [&](std::size_t &i, const std::string &flag, std::string &dst) -> bool {
    if (i + 1 >= args.size()) {
      std::fprintf(stderr, "%s 缺少值\n", flag.c_str());
      return false;
    }
    dst = args[++i];
    return true;
  };

  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string &a = args[i];
    if (no_more_flags) { prompt_parts.push_back(a); continue; } // "--" 之後全是 prompt
    if (a == "--") { no_more_flags = true; continue; }          // 分隔符本身不進 prompt
    if (a == "--help" || a == "-h") {
      flags::print_usage();
      return kExitOk;
    }
    if (a == "--stream") {
      stream = true;
      continue;
    }
    if (a == "--image") {
      std::string v;
      if (!need_value(i, a, v))
        return kExitUsage;
      image_paths.push_back(v);
      continue;
    }
    if (a == "--schema") {
      if (!need_value(i, a, schema_path))
        return kExitUsage;
      has_schema = true;
      continue;
    }
    if (a == "--config") {
      if (!need_value(i, a, config_path))
        return kExitUsage;
      has_config = true;
      continue;
    }
    if (auto it = flag_to_field.find(a); it != flag_to_field.end()) {
      std::string v;
      if (!need_value(i, a, v))
        return kExitUsage;
      raw_values[it->second] = v;
      continue;
    }
    if (a.size() >= 1 && a[0] == '-' && a != "-") { // 「-」單獨當位置參數（stdin 慣例），其餘 - 開頭＝未知旗標
      std::fprintf(stderr, "未知旗標：%s\n", a.c_str());
      flags::print_usage();
      return kExitUsage;
    }
    prompt_parts.push_back(a); // 位置參數
  }

  // ── (2) prompt：有位置參數就拼（空格）；否則且 stdin 非終端才整段讀 ──
  std::string prompt;
  if (!prompt_parts.empty()) {
    for (std::size_t k = 0; k < prompt_parts.size(); ++k) {
      if (k)
        prompt += ' ';
      prompt += prompt_parts[k];
    }
  } else if (!stdin_is_tty()) {
    std::ostringstream ss;
    ss << std::cin.rdbuf(); // 只在 stdin 被導管/檔案餵入時整段讀（互動終端不讀，避免卡住）
    prompt = ss.str();
    while (!prompt.empty() && (prompt.back() == '\n' || prompt.back() == '\r'))
      prompt.pop_back(); // 去掉尾端換行，避免多餘空白進 prompt
  }
  // 互動終端且沒給位置參數 → prompt 仍空 → 落到下方報「缺 prompt」，不卡在讀取。
  if (prompt.empty()) {
    std::fprintf(stderr, "缺少 prompt：給位置參數或從 stdin 餵入\n");
    flags::print_usage();
    return kExitUsage;
  }

  // ── (3) 組 Client：內建預設 → config 檔 → 反射旗標 coerce 覆寫 ──
  llm::abi::Client client;
  if (int ec = config::load_into(client, has_config, config_path); ec != kExitOk)
    return ec;

  // 反射把 raw_values coerce 進對應欄位（覆寫 config）
  bool coerce_err = false;
  std::size_t idx = 0;
  glz::for_each_field(client, [&](auto &&f) {
    std::string key(glz::reflect<llm::abi::Client>::keys[idx++]);
    auto it = raw_values.find(key);
    if (it == raw_values.end())
      return;
    try {
      flags::assign_field(f, flags::kebab_flag(key), it->second);
    } catch (const std::exception &e) {
      std::fprintf(stderr, "%s\n", e.what());
      coerce_err = true;
    }
  });
  if (coerce_err)
    return kExitUsage;

  // ── (4) 組 Request（prompt＋stream＋schema 檔＋image 檔）──
  llm::abi::Request req;
  req.prompt = prompt;
  req.stream = stream;
  if (has_schema) {
    std::string body, err;
    if (!config::read_file(schema_path, body, err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return kExitUsage;
    }
    req.schema = llm::abi::Schema{.name = "response", .json = body};
  }
  for (const auto &p : image_paths) {
    std::string bytes, err;
    if (!config::read_file(p, bytes, err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return kExitUsage;
    }
    req.media.push_back(llm::abi::MediaIn{.url = "", .mime = config::mime_of(p), .bytes = std::move(bytes)});
  }

  // ── 出口 handlers：文字吐 stdout（串流逐段即時 flush；非串流一次整段）；錯誤吐 stderr ──
  bool wrote_any = false;
  llm::abi::Handlers h;
  h.on_text = [&](std::string_view sv) {
    std::fwrite(sv.data(), 1, sv.size(), stdout);
    std::fflush(stdout);
    wrote_any = true;
    return false; // 不中止
  };
  h.on_error = [&](std::string_view msg) {
    std::fprintf(stderr, "請求失敗：%.*s\n", static_cast<int>(msg.size()), msg.data());
  };
  // on_tool/on_media 不設：CLI 不開 tools／modalities，這兩路不會觸發。

  // ── 發問（可從 SIGINT 取消）──
  llm::abi::Context ctx;
  g_ctx.store(&ctx);
#ifndef _WIN32
  std::signal(SIGINT, on_sigint);
#endif
  llm::abi::Status st = client.ask(req, h, &ctx);
  g_ctx.store(nullptr);

  if (wrote_any && st == llm::abi::Status::Ok)
    std::fputc('\n', stdout); // 補尾端換行（stdout 收尾乾淨）

  switch (st) {
  case llm::abi::Status::Ok:
    return kExitOk;
  case llm::abi::Status::Cancelled:
    std::fprintf(stderr, "\n已取消\n");
    return kExitCancel;
  default: // Error：on_error 已印診斷
    return kExitRequest;
  }
}

} // namespace cli
