// http.hpp — cllm：native（C++）HTTP 傳輸的介面（傳統 header）。
//
// 對照 try_2 的 native/http.c（C＋Lua 綁定），這裡是**純 C++**——拋開 Lua C API，
// API 縫乾淨：struct 進出、傳輸失敗 throw std::runtime_error、串流回呼用 std::function。
// 分層原則與 try_2 一致：C++ 是「笨管子」——只管 TLS＋HTTP round-trip、串流時逐塊把
// raw bytes 交給回呼；SSE 拆框／UTF-8 分批／JSON 編解／ask 縫，全留給上層。
//
// ★ 系統標頭（windows.h／winhttp.h／curl.h）只在 http.cpp include，不外洩到本介面。

#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace http {

struct Request {
    std::string url;
    std::string method = "POST";
    std::vector<std::string> headers;   // 每條完整 "Key: Value"
    std::string body;
    long timeout_ms = 0;                 // 0＝不設逾時
};

struct Response {
    int status = 0;
    std::string body;
};

// 每收到一塊 raw bytes 呼一次；回 true 表示中止傳輸（false＝繼續）。
using OnData = std::function<bool(std::string_view)>;

Response request(const Request& req);                    // 非串流：累積整包 body（失敗 throw）
int stream(const Request& req, const OnData& on_data);   // 串流：內容走 on_data，回 status

}  // namespace http
