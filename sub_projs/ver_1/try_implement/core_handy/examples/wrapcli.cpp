// examples/wrapcli.cpp — 垂直切片：把既有 CLI（tr a-z A-Z）馴化成可靠的 ac_helper
//
// 目的：把目前所有非 LLM 設施「串成一個真工具」跑一次，驗證它們能協作。
// 這正是 roadmap「LLM Entry Manager」的架構雛形——只是核心暫用確定性的 shell-out
// 代替 LLM：同一支程式既能 one-shot、也能長駐成 warm server。
//
// 串起的設施：
//   intercept/meta_json（--metadata 自我描述）、cli.resolve/is_dry_run、io（batch 讀寫）、
//   shell.run（wrap 既有 CLI）、serve.serve_socket（長駐）、state.StateStore（跨呼叫/跨程序計數）。
//
// 用法：
//   wrapcli --metadata          → 吐九軸 JSON 並 exit
//   echo abc | wrapcli          → ABC                （one-shot：讀 stdin → wrap tr → 寫 stdout）
//   echo abc | wrapcli --dry-run→ abc（原樣，不真執行轉換）
//   wrapcli --serve <sock>      → 長駐 server：每連線一請求做同樣轉換，計數持久於 StateStore
#include <iostream>
#include <string>

#include "ac_helper.hpp"

namespace {

// 馴化的本體：把一段輸入交給既有 CLI（tr a-z A-Z）轉大寫。
// 真 wrapper 會檢查 exit code / stderr；切片從簡，只取 stdout。
std::string transform(const std::string& in) {
  return ac::shell::run({"tr", "a-z", "A-Z"}, in).out;
}

}  // namespace

int main(int argc, char** argv) {
  // 自我描述：讀 stdin(text)、寫 stdout(text)，batch；stateful（server 記計數）；冪等；支援乾跑。
  ac::Meta meta;
  meta.entries["stdin"]  = {ac::Entry::in,  ac::Entry::batch, ac::Entry::text};
  meta.entries["stdout"] = {ac::Entry::out, ac::Entry::batch, ac::Entry::text};
  meta.stateful      = true;
  meta.guarantee     = ac::Guarantee::idempotent;
  meta.allow_dry_run = true;
  ac::intercept(argc, argv, meta);  // 命中 --metadata → 吐 JSON + exit

  const bool dry = ac::cli::is_dry_run(argc, argv);

  // ── server 模式：把 transform 託管成長駐 server，計數持久於 StateStore（跨請求＋跨程序）。
  if (const std::string sock = ac::cli::resolve(argc, argv, "--serve", ""); !sock.empty()) {
    ac::state::StateStore store{"wrapcli"};
    std::cerr << "wrapcli serving on " << sock << "（SIGINT/SIGTERM 結束）\n";
    ac::serve::serve_socket(sock, [&store, dry](std::string req) {
      long n = store.exists(ac::state::Dir::state)
                   ? std::stol(store.load(ac::state::Dir::state)) : 0;
      ++n;
      if (!dry) store.save(ac::state::Dir::state, std::to_string(n));  // 原子持久
      return "#" + std::to_string(n) + " " + (dry ? req : transform(req));
    });
    return 0;  // 不可達
  }

  // ── one-shot 模式：batch 讀 stdin → wrap → 寫 stdout。
  const std::string in = ac::io::read_all("-");
  ac::io::write_all("-", dry ? in : transform(in));
  return 0;
}
