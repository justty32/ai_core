// impl/shell.hpp — 軸 1 #11 / brownfield-wrap：spawn 子程序、餵 stdin、收 stdout/stderr + exit code
//
// roadmap「馴化既有 CLI」的入口：greenfield ac_helper 程式以 wrapper 身分把既有程式
// （git/rsync/terraform…）包進系統——wrapper 的本職就是 subprocess 管理（見 notes/00_index.md
// 全域立場 brownfield=greenfield-wrap）。是軸 1 pipe 的組合/特化。
//
// 死結防護：用 poll 多工——同時「寫 stdin（非阻塞）/ 讀 stdout / 讀 stderr」，
//   避免「父寫 stdin 阻塞 ↔ 子寫 stdout 阻塞」互鎖。stdin 設 O_NONBLOCK、out/err 僅在 POLLIN 後讀。
// 範圍：execvp（走 PATH）。shell 注入安全：argv 向量直傳、不經 /bin/sh（要 shell 自行 {"sh","-c",...}）。
#pragma once

#include <csignal>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ac::shell {

struct Result {
  std::string out;    // 子程序 stdout
  std::string err;    // 子程序 stderr
  int code = -1;      // 正常結束＝exit code；被 signal 殺＝128+signo；exec 失敗＝127
};

// 執行 cmd（cmd[0]=程式名，走 PATH）；input 餵其 stdin；回收 stdout/stderr 與 exit code。
inline Result run(const std::vector<std::string>& cmd, std::string_view input = "") {
  if (cmd.empty()) throw std::runtime_error("ac::shell::run: 空命令");
  std::signal(SIGPIPE, SIG_IGN);  // 子早關 stdin 時，寫端改回傳 -1 而非殺掉父

  int in_p[2], out_p[2], err_p[2];
  if (::pipe(in_p) || ::pipe(out_p) || ::pipe(err_p))
    throw std::runtime_error("ac::shell::run: pipe() 失敗");

  const pid_t pid = ::fork();
  if (pid < 0) throw std::runtime_error("ac::shell::run: fork() 失敗");

  if (pid == 0) {
    // 子：接管 0/1/2，關所有 pipe fd，exec。
    ::dup2(in_p[0], 0);
    ::dup2(out_p[1], 1);
    ::dup2(err_p[1], 2);
    for (int fd : {in_p[0], in_p[1], out_p[0], out_p[1], err_p[0], err_p[1]}) ::close(fd);
    std::vector<char*> argv;
    argv.reserve(cmd.size() + 1);
    for (const auto& s : cmd) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);
    ::execvp(argv[0], argv.data());
    ::_exit(127);  // exec 失敗（命令不存在等）
  }

  // 父：關子端，留自己這端。
  ::close(in_p[0]);
  ::close(out_p[1]);
  ::close(err_p[1]);
  int wfd = in_p[1], ofd = out_p[0], efd = err_p[0];
  ::fcntl(wfd, F_SETFL, ::fcntl(wfd, F_GETFL, 0) | O_NONBLOCK);  // stdin 非阻塞

  Result r;
  std::size_t written = 0;
  bool win_open = !input.empty(), out_open = true, err_open = true;
  if (!win_open) ::close(wfd);  // 無 input → 直接關 stdin 給子 EOF

  while (win_open || out_open || err_open) {
    struct pollfd fds[3];
    int wi = -1, oi = -1, ei = -1, nf = 0;
    if (win_open) { fds[nf] = {wfd, POLLOUT, 0}; wi = nf++; }
    if (out_open) { fds[nf] = {ofd, POLLIN, 0};  oi = nf++; }
    if (err_open) { fds[nf] = {efd, POLLIN, 0};  ei = nf++; }
    if (::poll(fds, static_cast<nfds_t>(nf), -1) < 0) break;

    if (win_open && (fds[wi].revents & (POLLOUT | POLLERR | POLLHUP))) {
      const ssize_t n = ::write(wfd, input.data() + written, input.size() - written);
      if (n > 0) written += static_cast<std::size_t>(n);
      if (n < 0 || written >= input.size()) { ::close(wfd); win_open = false; }
    }
    if (out_open && (fds[oi].revents & (POLLIN | POLLHUP | POLLERR))) {
      char b[65536];
      const ssize_t n = ::read(ofd, b, sizeof b);
      if (n > 0) r.out.append(b, static_cast<std::size_t>(n));
      else { ::close(ofd); out_open = false; }
    }
    if (err_open && (fds[ei].revents & (POLLIN | POLLHUP | POLLERR))) {
      char b[65536];
      const ssize_t n = ::read(efd, b, sizeof b);
      if (n > 0) r.err.append(b, static_cast<std::size_t>(n));
      else { ::close(efd); err_open = false; }
    }
  }

  int status = 0;
  ::waitpid(pid, &status, 0);
  if (WIFEXITED(status)) r.code = WEXITSTATUS(status);
  else if (WIFSIGNALED(status)) r.code = 128 + WTERMSIG(status);
  return r;
}

}  // namespace ac::shell
