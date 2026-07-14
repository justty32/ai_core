// http.cppm — galtxt try_3：native（C++）HTTP 傳輸模組。
//
// 對照 try_2 的 native/http.c（C＋Lua 綁定），這裡是**純 C++20 named module**——
// 拋開 Lua C API，API 縫變乾淨：struct 進出、傳輸失敗 throw std::runtime_error、
// 串流回呼用 std::function。分層原則與 try_2 一致：
//   C++ 是「笨管子」——只管 TLS＋HTTP round-trip、串流時逐塊把 raw bytes 交給回呼；
//   SSE 拆框／UTF-8 分批／JSON 編解／ask 縫，全留給上層（改起來便宜、傳輸層語言中立）。
//
// 平台（比照 main.cpp 的 #ifdef 分流）：
//   Windows：WinHTTP（系統內建、Schannel TLS，零額外依賴；連結 -lwinhttp）。
//   其它（Linux/Mac）：libcurl（vcpkg 裝 curl、連結 CURL::libcurl）。
//   兩平台共用 file:// 特例：直接讀檔當 200 回應——保住離線 fixture 測試 harness
//   （WinHTTP 不支援 file://，故非在這層處理不可）。
//
// ★ modules 關鍵：傳統系統標頭（windows.h／winhttp.h／curl.h）必須放在「全域模組片段」
//   （module; 之後、export module 之前）#include，模組本體只 export 對外介面。
//
// API：
//   http::Request { url, method="POST", headers{"K: V",…}, body, timeout_ms }
//   http::Response { status:int, body:string }
//   http::request(req) -> Response                （非串流；傳輸失敗 throw）
//   http::stream(req, on_data) -> int(status)     （內容經 on_data 逐塊餵；回 false 中止）

module;                 // ── 全域模組片段：傳統 header 在這裡 include
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#endif

export module http;     // ── 具名模組 http 的主介面單元

// ══════════════════════════ 對外介面（export） ══════════════════════════
export namespace http {

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

// 每收到一塊 raw bytes 呼一次；回 false 表示中止傳輸。
using OnData = std::function<bool(std::string_view)>;

Response request(const Request& req);              // 非串流：累積整包 body
int stream(const Request& req, const OnData& on_data);  // 串流：內容走 on_data，回 status

}  // namespace http

// ══════════════════════════ 模組內部（不 export） ══════════════════════════
namespace {

// 傳輸情境：串流→把每塊交給 on_data；非串流→累積進 buf。
struct Sink {
    bool streaming = false;
    const http::OnData* cb = nullptr;   // 串流時指向使用者回呼
    std::string buf;                    // 非串流時累積本體
    bool aborted = false;

    // 交付一塊收到的 bytes。回傳是否應繼續（false＝中止）。
    bool deliver(const char* p, size_t n) {
        if (aborted) return false;
        if (streaming) {
            bool cont = (*cb)(std::string_view(p, n));
            if (!cont) aborted = true;
            return cont;
        }
        buf.append(p, n);
        return true;
    }
};

// file:// 特例：讀整個檔當 200 回應本體。讀不到→throw。
int do_file(const std::string& url, Sink& sink) {
    const char* path = url.c_str() + 7;              // 跳過 "file://"
    // Windows 的 "file:///C:/…" 去掉開頭那個 '/'；Linux 的 "/home/…" 保留
    if (path[0] == '/' && path[1] && path[2] == ':') path += 1;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) throw std::runtime_error(std::string("http: 開不了 file:// 路徑：") + path);
    char tmp[8192];
    size_t r;
    while ((r = std::fread(tmp, 1, sizeof tmp, f)) > 0) {
        if (!sink.deliver(tmp, r)) break;
    }
    std::fclose(f);
    return 200;
}

// ── 平台實作：一次 HTTP 交易，收到的 bytes 逐塊丟給 sink.deliver ──
#ifdef _WIN32

std::wstring u8_to_w(const char* s, int len) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, len, nullptr, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s, len, w.data(), n);
    return w;
}

int do_http(const http::Request& req, Sink& sink) {
    std::wstring wurl = u8_to_w(req.url.c_str(), -1);
    URL_COMPONENTS uc;
    wchar_t host[256], path[8192];
    std::memset(&uc, 0, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 8192;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
        throw std::runtime_error("http: URL 解析失敗：" + req.url);

    HINTERNET hS = WinHttpOpen(L"galtxt-try3/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hS) throw std::runtime_error("http: WinHttpOpen 失敗");
    if (req.timeout_ms > 0)
        WinHttpSetTimeouts(hS, req.timeout_ms, req.timeout_ms, req.timeout_ms, req.timeout_ms);

    HINTERNET hC = WinHttpConnect(hS, host, uc.nPort, 0);
    std::wstring wmethod = u8_to_w(req.method.c_str(), -1);
    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hR = hC ? WinHttpOpenRequest(hC, wmethod.c_str(), path, nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags)
                      : nullptr;
    if (!hR) {
        if (hC) WinHttpCloseHandle(hC);
        WinHttpCloseHandle(hS);
        throw std::runtime_error("http: 建立請求失敗");
    }

    // 標頭：WinHTTP 要一整條以 \r\n 串接的字串
    if (!req.headers.empty()) {
        std::string joined;
        for (const auto& h : req.headers) { joined += h; joined += "\r\n"; }
        std::wstring wh = u8_to_w(joined.c_str(), -1);
        WinHttpAddRequestHeaders(hR, wh.c_str(), (DWORD)-1,
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    BOOL ok = WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 (LPVOID)(req.body.empty() ? nullptr : req.body.data()),
                                 (DWORD)req.body.size(), (DWORD)req.body.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(hR, nullptr);
    if (!ok) {
        WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
        throw std::runtime_error("http: 送出/接收失敗——後端沒開？");
    }

    DWORD status = 0, sz = sizeof status;
    WinHttpQueryHeaders(hR, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hR, &avail) || avail == 0) break;
        std::vector<char> tmp(avail);
        DWORD got = 0;
        if (!WinHttpReadData(hR, tmp.data(), avail, &got) || got == 0) break;
        if (!sink.deliver(tmp.data(), got)) break;
    }

    WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
    return (int)status;
}

#else  // ── 非 Windows：libcurl ──

size_t curl_write(char* ptr, size_t size, size_t nmemb, void* ud) {
    Sink* s = static_cast<Sink*>(ud);
    size_t n = size * nmemb;
    return s->deliver(ptr, n) ? n : 0;   // 回傳 <n 會讓 libcurl 中止
}

int do_http(const http::Request& req, Sink& sink) {
    CURL* h = curl_easy_init();
    if (!h) throw std::runtime_error("http: curl_easy_init 失敗");
    curl_easy_setopt(h, CURLOPT_URL, req.url.c_str());
    if (req.method == "POST") {
        curl_easy_setopt(h, CURLOPT_POSTFIELDS, req.body.c_str());
        curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, (long)req.body.size());
    } else {
        curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, req.method.c_str());
    }
    struct curl_slist* hs = nullptr;
    for (const auto& hdr : req.headers) hs = curl_slist_append(hs, hdr.c_str());
    if (hs) curl_easy_setopt(h, CURLOPT_HTTPHEADER, hs);
    if (req.timeout_ms > 0) curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, req.timeout_ms);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode rc = curl_easy_perform(h);
    long status = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
    if (hs) curl_slist_free_all(hs);
    curl_easy_cleanup(h);
    if (rc != CURLE_OK && !sink.aborted)
        throw std::runtime_error(std::string("http: 傳輸失敗——") + curl_easy_strerror(rc));
    return (int)status;
}

#endif

// 共用進入點：分派 file:// vs 真傳輸，包裝成 Response。
http::Response run(const http::Request& req, bool streaming, const http::OnData* cb) {
    Sink sink;
    sink.streaming = streaming;
    sink.cb = cb;
    int status;
    if (req.url.rfind("file://", 0) == 0) {
        status = do_file(req.url, sink);
    } else {
        status = do_http(req, sink);
    }
    http::Response r;
    r.status = status;
    r.body = std::move(sink.buf);
    return r;
}

}  // anonymous namespace

// ══════════════════════════ 介面定義 ══════════════════════════
http::Response http::request(const Request& req) {
    return run(req, false, nullptr);
}

int http::stream(const Request& req, const OnData& on_data) {
    return run(req, true, &on_data).status;
}
