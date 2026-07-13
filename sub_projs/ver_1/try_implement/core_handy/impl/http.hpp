// impl/http.hpp — 零相依 HTTP/1.1 client（raw POSIX socket）
//
// LLM 呼叫路徑的地基（對應 Python 線 lib/call.Http 的 urllib）。
// 範圍：
//   - **明文 http://**：raw POSIX socket（零相依）——服務本地小模型（ollama/llama.cpp/vLLM）。
//   - **https://**：C++ 標準庫無 TLS → shell-out 給 curl（零 library 相依；2026-06-28 使用者定）。
//     runtime 依賴 curl 二進位；dogfood ac::shell::run。
// 簡化：送 `Connection: close`，讀到 EOF 即整個回應——免處理 chunked / keep-alive。
#pragma once

#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "shell.hpp"  // https 走 curl shell-out（C++ 標準庫無 TLS）

namespace ac::http {

struct Response {
  int status = 0;        // HTTP 狀態碼（解析失敗＝0）
  std::string body;      // 回應主體
};

using Headers = std::vector<std::pair<std::string, std::string>>;

namespace detail {

// 拆 http://host[:port]/path → {host, port, path}。非 http:// 即丟例外。
struct Url { std::string host, port, path; };
inline Url parse_url(const std::string& url) {
  const std::string pfx = "http://";
  if (url.rfind("https://", 0) == 0)
    throw std::runtime_error("ac::http: 不支援 https（C++ 標準庫無 TLS；雲端請走 curl shell-out）");
  if (url.rfind(pfx, 0) != 0)
    throw std::runtime_error("ac::http: 只接受 http:// URL");
  const std::string rest = url.substr(pfx.size());
  const std::size_t slash = rest.find('/');
  const std::string authority = rest.substr(0, slash);
  Url u;
  u.path = (slash == std::string::npos) ? "/" : rest.substr(slash);
  const std::size_t colon = authority.find(':');
  if (colon == std::string::npos) { u.host = authority; u.port = "80"; }
  else { u.host = authority.substr(0, colon); u.port = authority.substr(colon + 1); }
  return u;
}

}  // namespace detail

// POST body 到 http:// url；回 {status, body}。零相依、阻塞、單發。
inline Response post(const std::string& url, const std::string& body,
                     const Headers& headers = {}) {
  std::signal(SIGPIPE, SIG_IGN);

  // https：C++ 標準庫無 TLS → shell-out 給 curl（零 library 相依，dogfood shell::run）。
  if (url.rfind("https://", 0) == 0) {
    std::vector<std::string> cmd = {"curl", "-sS", "--data-binary", "@-",
                                    "-w", "\n%{http_code}"};
    for (const auto& [k, v] : headers) { cmd.push_back("-H"); cmd.push_back(k + ": " + v); }
    cmd.push_back(url);
    const ac::shell::Result cr = ac::shell::run(cmd, body);
    if (cr.code != 0)
      throw std::runtime_error("ac::http: curl 失敗（exit " + std::to_string(cr.code) +
                               "）：" + cr.err);
    Response r;
    const std::size_t nl = cr.out.rfind('\n');  // 末行＝%{http_code}
    if (nl != std::string::npos) {
      r.status = std::atoi(cr.out.c_str() + nl + 1);
      r.body = cr.out.substr(0, nl);
    } else {
      r.body = cr.out;
    }
    return r;
  }

  const detail::Url u = detail::parse_url(url);

  // 解析位址 → 連 TCP。
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  if (::getaddrinfo(u.host.c_str(), u.port.c_str(), &hints, &res) != 0 || !res)
    throw std::runtime_error("ac::http: getaddrinfo 失敗（" + u.host + ":" + u.port + "）");
  int fd = -1;
  for (addrinfo* p = res; p; p = p->ai_next) {
    fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (fd < 0) continue;
    if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
    ::close(fd);
    fd = -1;
  }
  ::freeaddrinfo(res);
  if (fd < 0) throw std::runtime_error("ac::http: connect 失敗（" + u.host + ":" + u.port + "）");

  // 組請求。
  std::string req = "POST " + u.path + " HTTP/1.1\r\nHost: " + u.host +
                    "\r\nConnection: close\r\nContent-Length: " +
                    std::to_string(body.size()) + "\r\n";
  for (const auto& [k, v] : headers) req += k + ": " + v + "\r\n";
  req += "\r\n" + body;

  // 全送。
  std::size_t off = 0;
  while (off < req.size()) {
    const ssize_t n = ::write(fd, req.data() + off, req.size() - off);
    if (n <= 0) { ::close(fd); throw std::runtime_error("ac::http: write 失敗"); }
    off += static_cast<std::size_t>(n);
  }

  // 讀到 EOF（Connection: close）。
  std::string raw;
  char buf[65536];
  for (;;) {
    const ssize_t n = ::read(fd, buf, sizeof buf);
    if (n > 0) raw.append(buf, static_cast<std::size_t>(n));
    else break;
  }
  ::close(fd);

  // 解析：狀態行 + 切頭/身。
  Response r;
  const std::size_t sp = raw.find(' ');
  if (sp != std::string::npos) r.status = std::atoi(raw.c_str() + sp + 1);
  const std::size_t sep = raw.find("\r\n\r\n");
  r.body = (sep == std::string::npos) ? std::string() : raw.substr(sep + 4);
  return r;
}

}  // namespace ac::http
