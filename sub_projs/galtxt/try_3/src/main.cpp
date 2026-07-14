// main.cpp — galtxt try_3 進入點：import 具名模組 demo、跑其邏輯
//
// try_3 是 galtxt 第三條實驗線：與 try_1（s7 Scheme）、try_2（C++ 內嵌 Lua）並存、互為對照。
// 這條**完全 C++**——不嵌任何腳本 VM，純原生，並用 **C++20 modules**（import demo;）當骨架示範。
//
// 跨平台：Windows 用 wmain + -municode 取寬字元命令列（中文 argv 不亂碼），
//         並 SetConsoleOutputCP(CP_UTF8) 讓主控台照 UTF-8 讀我們吐的位元組；
//         Linux/Mac 走標準 main、原生 UTF-8 argv。Windows 專屬碼以 #ifdef _WIN32 包起。
//
// 建置／除錯：見同層 README.md 與 .vscode/。CLI 直接跑：build/try3.exe [N] [名字]

#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>   // 反射式 JSON↔struct（vcpkg 裝的，header-only、超快）

#ifdef _WIN32
#include <windows.h>
// 寬字元 → UTF-8（對齊 try_2 host.cpp 的 w2u8）
static std::string w2u8(const wchar_t* w) {
    if (!w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(n - 1, '\0');  // n 含結尾 NUL
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
#endif

import demo;             // ── 具名模組（提供 sum_to / greet）
import http;             // ── 具名模組（native HTTP 傳輸：request / stream，file:// 離線可跑）

// ── glaze JSON 示範：定義 struct，glaze 靠編譯期反射自動 JSON↔struct（不用寫任何映射巨集）
struct Character {
    std::string name;
    int affection{};
    std::vector<std::string> lines;
};

// ── OpenAI chat completion 回應的最小 struct（glaze 反射解碼用；只挑要的欄位）
struct ChatMessage { std::string role; std::string content; };
struct ChatChoice  { ChatMessage message; };
struct ChatCompletion { std::string id; std::vector<ChatChoice> choices; };

static void demo_json() {
    // struct → JSON（序列化）
    Character c{ "星野", 42, { "哼，才不是為你回答的", "……下不為例喔" } };
    auto j = glz::write_json(c);
    if (j) std::printf("[glaze] 序列化 => %s\n", j->c_str());

    // JSON → struct（解析，含中文）
    std::string src = R"({"name":"綾波","affection":88,"lines":["嗯","抱抱"]})";
    Character parsed{};
    auto ec = glz::read_json(parsed, src);
    if (!ec) {
        std::printf("[glaze] 解析 => name=%s affection=%d lines[0]=%s\n",
                    parsed.name.c_str(), parsed.affection, parsed.lines[0].c_str());
    } else {
        std::printf("[glaze] 解析失敗\n");
    }
}

// ── native HTTP 示範：off-line 走 file:// 讀假回應，串接 glaze 把 JSON 解成 struct、取台詞。
//    傳輸層（http 模組）＋反射 JSON（glaze）＝這條純 C++ 線的兩塊地基端到端跑通。
//    TRY3_SOURCE_DIR 由 CMake 注入（見 CMakeLists），故 fixture 路徑不寫死機器路徑。
static void demo_http() {
#ifdef TRY3_SOURCE_DIR
    std::string url = std::string("file://") + TRY3_SOURCE_DIR
                      + "/test/fixtures/fake/chat/completions";
    http::Request req{ .url = url, .method = "GET" };
    try {
        http::Response resp = http::request(req);
        std::printf("[http] file:// GET status=%d bytes=%zu\n",
                    resp.status, resp.body.size());
        // 傳輸拿到的 body 交給 glaze 反射解碼 → 取第一則台詞
        // ★ error_on_unknown_keys=false：只挑 struct 有的欄位，忽略回應裡多出的
        //   （object/created/model/usage/finish_reason…）；否則 glaze 預設遇未知鍵報錯。
        ChatCompletion cc{};
        auto ec = glz::read<glz::opts{ .error_on_unknown_keys = false }>(cc, resp.body);
        if (!ec && !cc.choices.empty()) {
            std::printf("[http] 解出台詞 => %s\n", cc.choices[0].message.content.c_str());
        } else {
            std::printf("[http] glaze 解碼失敗\n");
        }
    } catch (const std::exception& e) {
        std::printf("[http] 失敗：%s\n", e.what());
    }
#else
    std::printf("[http] 未注入 TRY3_SOURCE_DIR，跳過 file:// 示範\n");
#endif
}

// ── 串流示範：http::stream 逐塊把 raw bytes 交給回呼（C++ 是笨管子，SSE 拆框留上層）。
//    file:// 一次讀完＝單塊；真後端 SSE 才會多塊。這裡只驗回呼路徑通、能中止。
static void demo_http_stream() {
#ifdef TRY3_SOURCE_DIR
    std::string url = std::string("file://") + TRY3_SOURCE_DIR
                      + "/test/fixtures/fake_stream/chat/completions";
    http::Request req{ .url = url, .method = "GET" };
    int chunks = 0;
    std::size_t bytes = 0;
    try {
        int status = http::stream(req, [&](std::string_view part) {
            ++chunks;
            bytes += part.size();
            return true;   // 回 false 可中止
        });
        std::printf("[http] stream status=%d chunks=%d bytes=%zu\n", status, chunks, bytes);
    } catch (const std::exception& e) {
        std::printf("[http] stream 失敗：%s\n", e.what());
    }
#endif
}

static int run(const std::vector<std::string>& args) {
    // args[0]＝執行檔；args[1..]＝參數。第一個參數當作累加上限 N（預設 10），第二個當名字。
    int n = 10;
    if (args.size() >= 2) {
        n = std::atoi(args[1].c_str());
        if (n <= 0) n = 10;
    }
    std::string name = (args.size() >= 3) ? args[2] : "主人";

    std::string hello = greet(name);   // ← 來自 module demo
    long long total = sum_to(n);       // ← 來自 module demo

    std::printf("%s\n", hello.c_str());
    std::printf("1..%d 的和 = %lld\n", n, total);

    demo_json();          // ── glaze JSON 往返示範
    demo_http();          // ── native HTTP（file://）→ glaze 解碼取台詞
    demo_http_stream();   // ── native HTTP 串流（逐塊回呼）
    return 0;
}

#ifdef _WIN32
int wmain(int argc, wchar_t** wargv) {
    // 主控台改用 UTF-8：我們吐 UTF-8 位元組，終端碼頁若是 950/936 會被讀成亂碼。
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) args.push_back(w2u8(wargv[i]));
    return run(args);
}
#else
int main(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
    return run(args);
}
#endif
