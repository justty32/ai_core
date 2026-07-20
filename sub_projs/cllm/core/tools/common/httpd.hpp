// httpd.hpp — tools 共用的「微型 HTTP/1.1 伺服器」介面（傳統 header）。
//
// cllm 的 http（http.hpp）是**client**（WinHTTP／libcurl，只會發請求）；這兩支工具卻要
// **listen**（anthropic-proxy 收 cllm 的請求、llm-login 收 OAuth callback）。cllm 沒有 server，
// 故自寫這支 raw-socket 的笨伺服器：只管 accept→解請求→交 handler→回應（含 chunked 串流）。
// 分層與 http.hpp 對稱：這裡是笨管子，SSE 翻譯／OAuth 縫留給上層。
//
// ★ 系統標頭（sys/socket.h／winsock2.h）只在 httpd.cpp include，不外洩到本介面。
// 零外部依賴：POSIX socket ＋ Windows Winsock，用 #ifdef 包乾淨。

#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace httpd {

// 一筆進來的請求（已解好請求行＋標頭＋body）。
struct Request {
  std::string method;                                    // "POST"／"GET"
  std::string target;                                    // "/v1/chat/completions?a=b"（原樣）
  std::string path;                                      // "?" 之前
  std::string query;                                     // "?" 之後（原樣、未解碼）
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;

  std::string header(std::string_view key) const;        // 大小寫不敏感；沒有回 ""
  std::string query_param(std::string_view key) const;   // URL 解碼；沒有回 ""
};

// 回應寫手：非串流一次 send；串流 begin_chunked→write_chunk*→end_chunked。
// 綁著單一連線的 socket，handler 在該連線的 thread 內直接寫出。
class Responder {
public:
  explicit Responder(int fd) : fd_(fd) {}

  // 非串流：一次送完（自動補 Content-Length＋Connection: close）。
  void send(int status, std::string_view content_type, std::string_view body);
  void send_json(int status, std::string_view json) { send(status, "application/json", json); }

  // 串流：先送標頭（Transfer-Encoding: chunked），再逐塊寫，最後收尾。
  void begin_chunked(int status, std::string_view content_type);
  bool write_chunk(std::string_view data);  // 回 false＝對端已斷
  void end_chunked();

  bool headers_sent() const { return sent_; }

private:
  int fd_;
  bool sent_ = false;
  bool raw_write(std::string_view);         // 全寫出；對端斷回 false
};

// handler：拿到解好的 Request，用 Responder 回。丟例外會被 server 攔成 500。
using Handler = std::function<void(const Request &, Responder &)>;

// 監聽 host:port。port 傳 0＝由 OS 配，之後用 port() 取實際埠（嵌入用）。
// bind／listen 失敗 throw std::runtime_error。
class Server {
public:
  Server(const std::string &host, int port);
  ~Server();
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  int port() const { return port_; }

  void serve_forever(const Handler &);  // 阻塞 accept 迴圈，每連線一條 detached thread
  void serve_once(const Handler &);     // 只處理一個連線就回（OAuth callback 用）

private:
  int listen_fd_ = -1;
  int port_ = 0;
  void handle_conn(int fd, const Handler &);  // 解一筆請求→呼 handler
};

}  // namespace httpd
