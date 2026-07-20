// cli.cpp — 薄 CLI 外殼的 orchestrator＋main（對齊 core-py 的 cli.py／core/src/cli.cpp）。
//
// 流程：(1) 掃描 argv（argv::parse_argv）(2) 定 prompt（prompt::build）(3) 組 client：內建預設 →
//       config 檔（config::load_into）→ 反射旗標覆寫（flags::apply_client_flags）(4) 組 Request＋
//       呼叫 binding 的 llm::Client::ask（聚合＋expected）→ Sink 出口：文字 stdout、tool_calls
//       JSON 行、媒體落檔、錯誤 stderr。SIGINT → ctx.cancel() 取消在途請求（退 130）。

#include "cli.hpp"

#include <atomic>
#include <cstdio>
#include <string>

#ifndef _WIN32
#include <csignal>
#endif

#include <cllm/llm.hpp>

#include "argv.hpp"
#include "cli_config.hpp"
#include "cli_flags.hpp"
#include "cli_internal.hpp"
#include "cli_output.hpp"
#include "prompt.hpp"
#include "reqinput.hpp"

namespace cli {
namespace {
// ── SIGINT → 取消在途請求（POSIX）。handler 只做 atomic 存，async-signal 安全。──
std::atomic<llm::abi::Context *> g_ctx{nullptr};
#ifndef _WIN32
extern "C" void on_sigint(int) {
  if (auto *c = g_ctx.load())
    c->cancel();
}
#endif
} // namespace

int run(const std::vector<std::string> &args) {
  // ── (1) 掃描 argv ──
  argv::ParsedArgs p = argv::parse_argv(args);
  if (p.stop)
    return p.exit_code;

  // ── (2) prompt ──
  std::string prompt;
  int ec = kExitOk;
  if (!prompt::build(p, prompt, ec))
    return ec;

  // ── (3) 組 client：內建預設 → config 檔 → 反射旗標覆寫 ──
  llm::Client client;
  if (int rc = config::load_into(client, p.has_config, p.config_path); rc != kExitOk)
    return rc;
  if (!flags::apply_client_flags(client, p.raw_values))
    return kExitUsage;

  // ── (4) 組 Request（含請求類旗標四件組）──
  reqinput::RequestInputs ri;
  if (int rc = reqinput::build_request_inputs(p, ri); rc != kExitOk)
    return rc;
  llm::abi::Request req;
  req.prompt = prompt;
  req.stream = p.stream;
  if (p.has_system) req.system = p.system_text;
  req.schema = ri.schema;
  req.tools = std::move(ri.tools);
  req.media = std::move(ri.media);
  req.modalities = std::move(ri.modalities);

  // ── 發問（重用 binding 聚合＋expected；on_delta 逐段吐 stdout，可從 SIGINT 取消）──
  output::Sink sink{.media_out_dir = p.media_out_dir};
  llm::abi::Context ctx;
  g_ctx.store(&ctx);
#ifndef _WIN32
  std::signal(SIGINT, on_sigint);
#endif
  auto res = client.ask(req, {.stream = p.stream,
                              .on_delta = [&](std::string_view sv) { return sink.on_delta(sv); },
                              .ctx = &ctx});
  g_ctx.store(nullptr);

  if (!res) { // 傳輸／後端失敗或取消（錯誤原文由便利層從 on_error 收進 Error.message）
    if (res.error().status == llm::abi::Status::Cancelled) {
      std::fprintf(stderr, "\n已取消\n");
      return kExitCancel;
    }
    std::fprintf(stderr, "請求失敗：%s\n", res.error().message.c_str());
    return kExitRequest;
  }
  // 成功：補文字尾端換行 → tool_calls 一行一則 JSON → 產出媒體落檔。
  if (sink.wrote_text)
    std::fputc('\n', stdout);
  for (const auto &tc : res->tool_calls)
    sink.emit_tool_call(tc);
  for (const auto &m : res->media)
    sink.save_media(m);
  return sink.media_err ? kExitRequest : kExitOk; // 媒體落檔失敗＝結果真掉了
}

} // namespace cli

int main(int argc, char **argv) {
  return cli::run(std::vector<std::string>(argv, argv + argc));
}
