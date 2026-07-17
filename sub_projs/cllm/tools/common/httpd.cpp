// httpd.cpp — 微型 HTTP/1.1 伺服器實作（raw socket；POSIX＋Winsock，#ifdef 包乾淨）。
//
// 笨管子：accept→讀請求行＋標頭（到 \r\n\r\n）→依 Content-Length 讀 body→交 handler。
// 回應走 Responder（非串流補 Content-Length；串流用 chunked）。連線一律 Connection: close
// （cllm 的 http client 本就一次一請求，不需 keep-alive）。

#include "httpd.hpp"

#include <cstring>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#define CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCKET (-1)
#define CLOSESOCK ::close
#endif

namespace httpd {
namespace httpd_impl {  // 具名子 namespace（對齊專案慣例，不用匿名）

#ifdef _WIN32
struct WsaGuard {
  WsaGuard() { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); }
};
static WsaGuard g_wsa;  // 靜態初始化一次
#endif

std::string lower(std::string_view s) {
  std::string r(s);
  for (char &c : r)
    if (c >= 'A' && c <= 'Z') c += 32;
  return r;
}

int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string url_decode(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '+') {
      out.push_back(' ');
    } else if (s[i] == '%' && i + 2 < s.size()) {
      int hi = hexval(s[i + 1]), lo = hexval(s[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
      } else {
        out.push_back(s[i]);
      }
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

bool send_all(int fd, std::string_view data) {
  const char *p = data.data();
  size_t left = data.size();
  while (left > 0) {
    auto n = ::send(fd, p, static_cast<int>(left), 0);
    if (n <= 0) return false;
    p += n;
    left -= static_cast<size_t>(n);
  }
  return true;
}

// 從 socket 讀到收齊「\r\n\r\n」為止，回已讀全部（含 body 開頭殘料）；對端斷回空。
std::string read_headers(int fd, std::string &leftover) {
  std::string buf = leftover;
  char tmp[4096];
  while (buf.find("\r\n\r\n") == std::string::npos) {
    auto n = ::recv(fd, tmp, sizeof(tmp), 0);
    if (n <= 0) return {};
    buf.append(tmp, static_cast<size_t>(n));
    if (buf.size() > (1u << 22)) return {};  // 4MB 標頭上限、擋惡意
  }
  return buf;
}

}  // namespace httpd_impl
using namespace httpd_impl;

// ───────────────────────── Request ─────────────────────────
std::string Request::header(std::string_view key) const {
  std::string k = lower(key);
  for (const auto &[hk, hv] : headers)
    if (lower(hk) == k) return hv;
  return {};
}

std::string Request::query_param(std::string_view key) const {
  // query 形如 "a=b&code=xxx&state=yy"
  size_t i = 0;
  while (i < query.size()) {
    size_t amp = query.find('&', i);
    std::string_view kv = std::string_view(query).substr(
        i, amp == std::string::npos ? std::string::npos : amp - i);
    size_t eq = kv.find('=');
    std::string_view k = eq == std::string_view::npos ? kv : kv.substr(0, eq);
    if (url_decode(k) == key)
      return eq == std::string_view::npos ? "" : url_decode(kv.substr(eq + 1));
    if (amp == std::string::npos) break;
    i = amp + 1;
  }
  return {};
}

// ───────────────────────── Responder ─────────────────────────
bool Responder::raw_write(std::string_view s) { return send_all(fd_, s); }

void Responder::send(int status, std::string_view content_type, std::string_view body) {
  std::string h = "HTTP/1.1 " + std::to_string(status) + " \r\n";
  h += "Content-Type: " + std::string(content_type) + "\r\n";
  h += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  h += "Connection: close\r\n\r\n";
  sent_ = true;
  raw_write(h);
  raw_write(body);
}

void Responder::begin_chunked(int status, std::string_view content_type) {
  std::string h = "HTTP/1.1 " + std::to_string(status) + " \r\n";
  h += "Content-Type: " + std::string(content_type) + "\r\n";
  h += "Cache-Control: no-cache\r\n";
  h += "Transfer-Encoding: chunked\r\n";
  h += "Connection: close\r\n\r\n";
  sent_ = true;
  raw_write(h);
}

bool Responder::write_chunk(std::string_view data) {
  if (data.empty()) return true;
  char sz[24];
  int n = std::snprintf(sz, sizeof(sz), "%zX\r\n", data.size());
  if (!raw_write(std::string_view(sz, static_cast<size_t>(n)))) return false;
  if (!raw_write(data)) return false;
  return raw_write("\r\n");
}

void Responder::end_chunked() { raw_write("0\r\n\r\n"); }

// ───────────────────────── Server ─────────────────────────
Server::Server(const std::string &host, int port) {
  socket_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd == INVALID_SOCKET) throw std::runtime_error("socket() 失敗");
  int yes = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes),
               sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  addr.sin_addr.s_addr = host == "0.0.0.0" ? INADDR_ANY : inet_addr(host.c_str());

  if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
    CLOSESOCK(fd);
    throw std::runtime_error("bind " + host + ":" + std::to_string(port) + " 失敗");
  }
  if (::listen(fd, 64) != 0) {
    CLOSESOCK(fd);
    throw std::runtime_error("listen 失敗");
  }
  sockaddr_in bound{};
  socklen_t blen = sizeof(bound);
  if (::getsockname(fd, reinterpret_cast<sockaddr *>(&bound), &blen) == 0)
    port_ = ntohs(bound.sin_port);
  else
    port_ = port;
  listen_fd_ = static_cast<int>(fd);
}

Server::~Server() {
  if (listen_fd_ >= 0) CLOSESOCK(static_cast<socket_t>(listen_fd_));
}

void Server::handle_conn(int fd, const Handler &handler) {
  std::string leftover;
  std::string buf = read_headers(fd, leftover);
  Responder res(fd);
  if (buf.empty()) return;

  size_t hdr_end = buf.find("\r\n\r\n");
  std::string head = buf.substr(0, hdr_end);
  std::string body = buf.substr(hdr_end + 4);

  Request req;
  // 請求行：METHOD target HTTP/1.1
  size_t line_end = head.find("\r\n");
  std::string line = head.substr(0, line_end);
  size_t sp1 = line.find(' '), sp2 = line.find(' ', sp1 + 1);
  if (sp1 == std::string::npos || sp2 == std::string::npos) return;
  req.method = line.substr(0, sp1);
  req.target = line.substr(sp1 + 1, sp2 - sp1 - 1);
  size_t q = req.target.find('?');
  req.path = q == std::string::npos ? req.target : req.target.substr(0, q);
  req.query = q == std::string::npos ? "" : req.target.substr(q + 1);

  // 標頭
  size_t pos = line_end + 2;
  while (pos < head.size()) {
    size_t e = head.find("\r\n", pos);
    if (e == std::string::npos) e = head.size();
    std::string hline = head.substr(pos, e - pos);
    size_t c = hline.find(':');
    if (c != std::string::npos) {
      std::string k = hline.substr(0, c);
      std::string v = hline.substr(c + 1);
      while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
      req.headers.emplace_back(std::move(k), std::move(v));
    }
    pos = e + 2;
  }

  // body：依 Content-Length 補齊
  size_t clen = 0;
  std::string cl = req.header("Content-Length");
  if (!cl.empty()) clen = std::strtoul(cl.c_str(), nullptr, 10);
  char tmp[4096];
  while (body.size() < clen) {
    auto n = ::recv(fd, tmp, sizeof(tmp), 0);
    if (n <= 0) break;
    body.append(tmp, static_cast<size_t>(n));
  }
  req.body = std::move(body);

  try {
    handler(req, res);
  } catch (const std::exception &e) {
    if (!res.headers_sent())
      res.send_json(500, std::string("{\"error\":{\"message\":\"proxy 內部錯誤: ") +
                             e.what() + "\"}}");
  }
}

void Server::serve_forever(const Handler &handler) {
  for (;;) {
    socket_t fd = ::accept(static_cast<socket_t>(listen_fd_), nullptr, nullptr);
    if (fd == INVALID_SOCKET) continue;
    std::thread([this, fd, &handler] {
      handle_conn(static_cast<int>(fd), handler);
      CLOSESOCK(fd);
    }).detach();
  }
}

void Server::serve_once(const Handler &handler) {
  socket_t fd = ::accept(static_cast<socket_t>(listen_fd_), nullptr, nullptr);
  if (fd == INVALID_SOCKET) return;
  handle_conn(static_cast<int>(fd), handler);
  CLOSESOCK(fd);
}

}  // namespace httpd
