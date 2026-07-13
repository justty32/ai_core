// impl/serve.hpp — 軸 2 serve 設施：把 one-shot 本體託管成長駐 server
//
// 把 handler(input)->output 託管成 Unix domain socket daemon。
// 對應描述面軸 2 lifecycle persistent=true（顯式終止）；Python 線對應 lib/server.serve_socket。
//
// 模型（roadmap：LLM 單資源→佇列）：單執行緒、循序 accept——一次處理一個請求，
//   狀態跨請求存活於 process（handler 以閉包持有，如 warm model / 共用 RateMeter）。
// 協定：一條連線＝一個請求——client 送完 input 後 half-close(SHUT_WR)，
//   server 讀到 EOF → handler → 寫回 → 關閉連線。
// 範圍：AF_UNIX（本機 IPC）。tcp / fan-out / 多工延後（同 io.hpp stream 重機器的延後邏輯）。
// 終止：阻塞迴圈，以 SIGINT/SIGTERM 結束（軸 2 persistent＝顯式終止）。
#pragma once

#include <csignal>
#include <cstddef>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace ac::serve {

// 託管的本體：一段輸入 → 一段輸出（同 ac_core 函式模型 / llm_call）。
using Handler = std::function<std::string(std::string)>;

// 在 path 開 AF_UNIX socket，循序服務每個連線。阻塞不返回，以 signal 終止。
inline void serve_socket(const std::string& path, const Handler& handler) {
  std::signal(SIGPIPE, SIG_IGN);  // client 早關不該殺掉 server（寫失敗改回傳 -1）

  sockaddr_un addr{};
  if (path.size() >= sizeof(addr.sun_path))
    throw std::runtime_error("ac::serve: socket 路徑過長");

  ::unlink(path.c_str());  // 清掉同名殘留（否則 bind EADDRINUSE）

  const int srv = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (srv < 0) throw std::runtime_error("ac::serve: socket() 失敗");

  addr.sun_family = AF_UNIX;
  std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);

  if (::bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(srv);
    throw std::runtime_error("ac::serve: bind() 失敗");
  }
  if (::listen(srv, 16) < 0) {
    ::close(srv);
    throw std::runtime_error("ac::serve: listen() 失敗");
  }

  for (;;) {
    const int cli = ::accept(srv, nullptr, nullptr);
    if (cli < 0) continue;  // 被 signal 打斷等 → 再試

    // 讀整個請求：client half-close 後本端讀到 EOF。
    std::string input;
    char buf[65536];
    for (;;) {
      const ssize_t n = ::read(cli, buf, sizeof(buf));
      if (n > 0) input.append(buf, static_cast<std::size_t>(n));
      else break;  // 0=EOF（請求結束）；<0=錯誤
    }

    const std::string output = handler(std::move(input));

    // 全寫回（部分寫要續寫）。
    std::size_t off = 0;
    while (off < output.size()) {
      const ssize_t n = ::write(cli, output.data() + off, output.size() - off);
      if (n <= 0) break;  // SIGPIPE 已忽略；寫失敗就放棄此連線
      off += static_cast<std::size_t>(n);
    }
    ::close(cli);
  }
}

}  // namespace ac::serve
