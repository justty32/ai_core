// cli.cpp — cli::run 的 orchestrator：把命令列組成一次 llm::abi::Client::ask 的發問。
//
// 流程（各步驟的機制拆在旁邊的 TU）：
//   (1) 解析 argv：固定旗標（--stream/--image/--schema/--config/--tool/--modality/--media-out/
//       --help/--）特判；反射旗標（cli_flags::client_flags 的合法表）吃下一個 argv 收進
//       raw_values；其餘當位置參數拼 prompt。
//   (2) 定 prompt：位置參數與導管 stdin 可合體——「-」＝stdin 插入點；沒寫「-」而兩者都有
//       ＝prompt＋空行＋stdin（指令在前、資料在後）；互動終端一律不讀 stdin（避免卡住）。
//   (3) 組 Client：內建預設 → config 檔（cli_config::load_into）→ 反射把 raw_values coerce 覆寫。
//   (4) 組 Request（prompt＋stream＋schema/image/tool/modality 檔）→ client.ask()，output 走
//       handlers：文字吐 stdout、tool_calls 一行一則 JSON、媒體落檔 --media-out、錯誤吐 stderr。
//
// 反射旗標的機制（型別分派／--help）在 cli_flags.*；config/檔案在 cli_config.*；共用常數在 cli_internal.hpp。

#include "cli.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <filesystem>
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
#include "cli_config.hpp"   // read_file / mime_of / load_into / load_tool_def
#include "cli_flags.hpp"    // client_flags / print_usage / assign_field / kebab_flag
#include "cli_internal.hpp" // kExit* 退出碼
#include "cli_output.hpp"   // output::Sink（出口 handlers：stdout／落檔／stderr）

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
  std::vector<std::string> tool_paths;           // --tool（可重複）
  std::vector<std::string> modality_specs;       // --modality（可重複；「名」或「名=設定檔」）
  std::string schema_path, config_path, media_out_dir;
  bool has_schema = false, has_config = false, stream = false;
  bool no_more_flags = false; // 見到 "--" 後，其餘 argv 一律當 prompt（unix 慣例）

  // 需要吃下一個 argv 的取值小工具：缺值即報錯。
  auto need_value = [&](std::size_t &i, const std::string &flag, std::string &dst) -> bool {
    if (i + 1 >= args.size()) {
      std::fprintf(stderr, "%s 缺少值（llm --help 看用法）\n", flag.c_str());
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
    if (a == "--tool") {
      std::string v;
      if (!need_value(i, a, v))
        return kExitUsage;
      tool_paths.push_back(v);
      continue;
    }
    if (a == "--modality") {
      std::string v;
      if (!need_value(i, a, v))
        return kExitUsage;
      modality_specs.push_back(v);
      continue;
    }
    if (a == "--media-out") {
      if (!need_value(i, a, media_out_dir))
        return kExitUsage;
      continue;
    }
    if (auto it = flag_to_field.find(a); it != flag_to_field.end()) {
      std::string v;
      if (!need_value(i, a, v))
        return kExitUsage;
      raw_values[it->second] = v;
      continue;
    }
    if (a.size() >= 1 && a[0] == '-' && a != "-") { // 「-」單獨＝stdin 佔位符（進位置參數），其餘 - 開頭＝未知旗標
      std::fprintf(stderr, "未知旗標：%s（llm --help 看用法）\n", a.c_str());
      return kExitUsage;
    }
    prompt_parts.push_back(a); // 位置參數
  }

  // ── (2) prompt：位置參數與導管 stdin 可合體 ──
  //   「-」＝stdin 插入點（明指；stdin 須為導管/檔案）；沒寫「-」而兩者都有＝prompt＋空行＋stdin
  //   （指令在前、資料在後，`cat report.txt | llm 總結這份` 直覺成立）；只有其一＝用那一個。
  bool has_dash = false;
  for (const auto &p : prompt_parts)
    if (p == "-") { has_dash = true; break; }

  std::string stdin_text;
  if (!stdin_is_tty()) { // 只在 stdin 被導管/檔案餵入時整段讀（互動終端不讀，避免卡住）
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    stdin_text = ss.str();
    while (!stdin_text.empty() && (stdin_text.back() == '\n' || stdin_text.back() == '\r'))
      stdin_text.pop_back(); // 去掉尾端換行，避免多餘空白進 prompt
  } else if (has_dash) {
    std::fprintf(stderr, "「-」要從 stdin 讀，但 stdin 是互動終端——用導管/檔案餵入（llm --help 看用法）\n");
    return kExitUsage;
  }

  std::string prompt;
  for (std::size_t k = 0; k < prompt_parts.size(); ++k) {
    if (k)
      prompt += ' ';
    prompt += (prompt_parts[k] == "-") ? stdin_text : prompt_parts[k];
  }
  if (!has_dash && !stdin_text.empty())
    prompt = prompt.empty() ? stdin_text : prompt + "\n\n" + stdin_text;

  if (prompt.empty()) {
    std::fprintf(stderr, "缺少 prompt：給位置參數或從 stdin 餵入（llm --help 看用法）\n");
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
      std::fprintf(stderr, "%s（--schema）\n", err.c_str());
      return kExitUsage;
    }
    req.schema = llm::abi::Schema{.name = "response", .json = body};
  }
  for (const auto &p : image_paths) {
    std::string bytes, err;
    if (!config::read_file(p, bytes, err)) {
      std::fprintf(stderr, "%s（--image）\n", err.c_str());
      return kExitUsage;
    }
    req.media.push_back(llm::abi::MediaIn{.url = "", .mime = config::mime_of(p), .bytes = std::move(bytes)});
  }
  for (const auto &p : tool_paths) { // --tool：工具定義檔 → req.tools
    llm::abi::ToolDef td;
    std::string err;
    if (!config::load_tool_def(p, td, err)) {
      std::fprintf(stderr, "%s（--tool）\n", err.c_str());
      return kExitUsage;
    }
    req.tools.push_back(std::move(td));
  }
  for (const auto &spec : modality_specs) { // --modality：「名」或「名=設定檔」→ req.modalities
    auto eq = spec.find('=');
    llm::abi::Modality mod;
    mod.name = spec.substr(0, eq); // npos＝整串當名
    if (mod.name.empty()) {
      std::fprintf(stderr, "--modality 缺模態名（如 audio 或 audio=cfg.json）\n");
      return kExitUsage;
    }
    if (eq != std::string::npos) {
      std::string err;
      if (!config::read_file(spec.substr(eq + 1), mod.config, err)) {
        std::fprintf(stderr, "%s（--modality 的設定檔）\n", err.c_str());
        return kExitUsage;
      }
    }
    req.modalities.push_back(std::move(mod));
  }
  if (!media_out_dir.empty() && !std::filesystem::is_directory(media_out_dir)) {
    std::fprintf(stderr, "--media-out 不是可用目錄：%s\n", media_out_dir.c_str());
    return kExitUsage;
  }

  // ── 出口 handlers（機制在 cli_output.*）：文字吐 stdout、tool_calls 一行一則 JSON、
  //    產出媒體落檔 --media-out、錯誤吐 stderr。事後讀 sink 的狀態旗標定收尾。──
  output::Sink sink{.media_out_dir = media_out_dir};
  llm::abi::Handlers h = sink.handlers();

  // ── 發問（可從 SIGINT 取消）──
  llm::abi::Context ctx;
  g_ctx.store(&ctx);
#ifndef _WIN32
  std::signal(SIGINT, on_sigint);
#endif
  llm::abi::Status st = client.ask(req, h, &ctx);
  g_ctx.store(nullptr);

  if (sink.wrote_text && st == llm::abi::Status::Ok)
    std::fputc('\n', stdout); // 補尾端換行（stdout 收尾乾淨）

  switch (st) {
  case llm::abi::Status::Ok:
    return sink.media_err ? kExitRequest : kExitOk; // 媒體落檔失敗＝結果真掉了，不能報成功

  case llm::abi::Status::Cancelled:
    std::fprintf(stderr, "\n已取消\n");
    return kExitCancel;
  default: // Error：on_error 已印診斷
    return kExitRequest;
  }
}

} // namespace cli
