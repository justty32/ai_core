// main.cpp — galtxt try_3 進入點：include demo/http 兩個 header、跑其邏輯
//
// try_3 是 galtxt 第三條實驗線：與 try_1（s7 Scheme）、try_2（C++ 內嵌 Lua）並存、互為對照。
// 這條**完全 C++、純原生**——不嵌任何腳本 VM。（早期曾用 C++20 modules 當骨架，已回歸傳統 header。）
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

#include "cli.hpp"           // cli::run：由 llm::Client 欄位反射生成的 --flag CLI
#include "demo.hpp"          // sum_to / greet
#include "http.hpp"          // native HTTP 傳輸：request / stream（file:// 離線可跑）
#include "llm.hpp"           // llm::Client：struct＋反射 ask 接口
#include "llm_tool.hpp"      // llm::ask_tools：工具呼叫（function calling）
#include "llm_media.hpp"     // llm::ask_vision：多媒體/vision
#include "llm_json.hpp"      // llm::ask_as<T>：結構化輸出（丟 struct 進、拿 struct 出）

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
            return false;   // 回 true 可中止
        });
        std::printf("[http] stream status=%d chunks=%d bytes=%zu\n", status, chunks, bytes);
    } catch (const std::exception& e) {
        std::printf("[http] stream 失敗：%s\n", e.what());
    }
#endif
}

// ── llm::Client 示範：離線把 endpoint 指向 fixture（file:// 走 http 讀檔特例當 200 回應）。
//    非串流讀 fake/、串流讀 fake_stream/（SSE），驗 struct＋反射組請求／解回應整條通。
static void demo_llm() {
#ifdef TRY3_SOURCE_DIR
    std::string base = std::string("file://") + TRY3_SOURCE_DIR + "/test/fixtures/";
    try {
        // 非串流：一次收完、反射解析、取答覆
        llm::Client client{ .endpoint = base + "fake/chat/completions" };
        std::printf("[llm] 非串流 => %s\n", client.ask("你好").c_str());

        // 串流：SSE 逐段經 on_delta 餵出（笨管子逐塊、上層拆框）
        llm::Client streamer{ .endpoint = base + "fake_stream/chat/completions" };
        std::printf("[llm] 串流逐段 => ");
        std::string whole = streamer.ask("你好", true, [](std::string_view piece) {
            // string_view 非 null 結尾，printf 用 %.*s（帶長度）；每段 [] 框起看得出分段
            std::printf("[%.*s]", static_cast<int>(piece.size()), piece.data());
            return false;
        });
        std::printf("　合＝%s\n", whole.c_str());
    } catch (const std::exception& e) {
        std::printf("[llm] 失敗：%s\n", e.what());
    }
#endif
}

// ── 工具呼叫示範：工具參數用 struct 定義（make_tool 反射生成 schema），離線讀 fake_tool。
//    GetWeather 就是工具參數的「唯一真相源」——它既生成送出的 schema，也解回模型產生的 arguments。
struct GetWeather {
    std::string city;   // 城市名
    std::string unit;   // 溫度單位：celsius / fahrenheit
};

static void demo_tool() {
#ifdef TRY3_SOURCE_DIR
    std::string url = std::string("file://") + TRY3_SOURCE_DIR + "/test/fixtures/fake_tool/chat/completions";
    llm::Client client{ .endpoint = url };
    try {
        llm::Tool weather = llm::make_tool<GetWeather>("get_weather", "查詢某城市天氣");
        auto calls = llm::ask_tools(client, "東京天氣如何？", { weather });
        std::printf("[tool] 模型要求呼叫 %zu 個工具\n", calls.size());
        for (const auto& c : calls) {
            std::printf("[tool]   %s(%s)\n", c.name.c_str(), c.arguments.c_str());
            // arguments 是 JSON 字串 → 反射解回 GetWeather struct
            GetWeather args{};
            if (!glz::read_json(args, c.arguments))
                std::printf("[tool]   解出參數 => city=%s unit=%s\n", args.city.c_str(), args.unit.c_str());
        }
    } catch (const std::exception& e) {
        std::printf("[tool] 失敗：%s\n", e.what());
    }
#endif
}

// ── 多媒體示範：帶文字＋圖片發問（離線 endpoint 指 fake，回罐頭答覆）。
//    另外用 image_from_file 讀一個現成檔驗 base64 data URI 組得出來（內容不必是真圖）。
static void demo_media() {
#ifdef TRY3_SOURCE_DIR
    std::string root = std::string(TRY3_SOURCE_DIR);
    llm::Client client{ .endpoint = "file://" + root + "/test/fixtures/fake/chat/completions" };
    try {
        // 外部 URL 的圖：組多模態請求、離線拿罐頭答覆
        auto img = llm::image_from_url("https://example.com/cat.png");
        std::printf("[media] vision 答覆 => %s\n", llm::ask_vision(client, "這張圖是什麼？", { img }).c_str());

        // 本地檔 → base64 data URI（拿現成 fixture 當 bytes，只驗 data URI 組得出來）
        auto local = llm::image_from_file(root + "/test/fixtures/fake/chat/completions", "image/png");
        std::printf("[media] data URI 前綴 => %.30s… (共 %zu 字)\n", local.url.c_str(), local.url.size());
    } catch (const std::exception& e) {
        std::printf("[media] 失敗：%s\n", e.what());
    }
#endif
}

// ── 結構化輸出示範：丟 Character 這個 struct 進去、拿 Character 出來（離線讀 fake_json）。
//    Character 就是唯一真相源——反射生成送出的 schema（約束模型），回來的 JSON 再反射回它。
static void demo_structured() {
#ifdef TRY3_SOURCE_DIR
    std::string url = std::string("file://") + TRY3_SOURCE_DIR + "/test/fixtures/fake_json/chat/completions";
    llm::Client client{ .endpoint = url };
    try {
        if (auto c = llm::ask_as<Character>(client, "生成一個傲嬌女角色", "character")) {
            std::printf("[json] 結構化 => name=%s affection=%d lines[0]=%s\n",
                        c->name.c_str(), c->affection, c->lines.empty() ? "" : c->lines[0].c_str());
        } else {
            std::printf("[json] 解析失敗\n");
        }
    } catch (const std::exception& e) {
        std::printf("[json] 失敗：%s\n", e.what());
    }
#endif
}

// ── 真後端示範：打本機 LM Studio（llm::from_env()：AI_CORE_LLM_BASE_URL 預設
//    http://localhost:1234/v1，AI_CORE_LLM_MODEL 預設 local-model）。四種能力各跑一次：
//    ask 非串流、ask 串流、ask_as<T> 結構化輸出、ask_tools 工具呼叫，全繁體中文 prompt。
//
//    ⚠ 兩個真後端才會踩到的坑（離線 fixture 驗不出來，2026-07-14 實測）：
//    1) 掛載的是 reasoning 模型（google/gemma-4-e4b）：思考鏈放在 message.reasoning_content、
//       答案才在 message.content，**兩者共用同一份 max_tokens 預算**。這裡刻意「完全不設
//       max_tokens」（client.max_tokens 維持 nullopt），交後端用 context 上限——實測設一個
//       小值（如 600）會被 reasoning 吃光，content 變空字串。
//    2) 回應很慢（reasoning 要想，單次可能 5～30 秒）：llm::from_env() 給了 120 秒逾時
//       （client.timeout_ms），這裡逐步印進度字樣，讓人知道沒當掉、只是在等模型想。
static void demo_real() {
    llm::Client client = llm::from_env();
    std::printf("[real] 後端＝%s\n", client.endpoint.c_str());
    std::printf("[real] 模型＝%s（未設 max_tokens：交後端用 context 上限，避免 reasoning 吃光）\n",
                client.model ? client.model->c_str() : "(未設)");

    // 1) ask 非串流
    std::printf("[real] (1/4) ask 非串流……送出中，reasoning 模型可能要等 5~30 秒，請耐心等候\n");
    try {
        std::string ans = client.ask("用一句話介紹你自己，請用繁體中文回答。");
        std::printf("[real] 非串流答覆 => %s\n", ans.c_str());
    } catch (const std::exception& e) {
        std::printf("[real] 非串流失敗：%s\n", e.what());
    }

    // 2) ask 串流：逐段印出（SSE 拆框在 llm.cpp 裡做，這裡只管收 delta）
    std::printf("[real] (2/4) ask 串流……逐段印出，看到字持續跑代表還活著\n");
    try {
        std::printf("[real] 串流逐段 => ");
        std::string whole = client.ask(
            "請用繁體中文數到五，每個數字後面加句號。", true,
            [](std::string_view piece) {
                std::printf("%.*s", static_cast<int>(piece.size()), piece.data());
                std::fflush(stdout);   // 逐段即時吐出，不等緩衝
                return false;
            });
        std::printf("\n[real] 串流合＝%s\n", whole.c_str());
    } catch (const std::exception& e) {
        std::printf("[real] 串流失敗：%s\n", e.what());
    }

    // 3) ask_as<T> 結構化輸出：丟 Character 進、拿 Character 出（schema 走 schema_of<T>()，見 llm_schema.hpp）
    std::printf("[real] (3/4) ask_as<Character> 結構化輸出……送出中\n");
    try {
        if (auto c = llm::ask_as<Character>(client, "生成一個傲嬌女角色，繁體中文", "character")) {
            std::printf("[real] 結構化 => name=%s affection=%d lines[0]=%s\n",
                        c->name.c_str(), c->affection, c->lines.empty() ? "" : c->lines[0].c_str());
        } else {
            std::printf("[real] 結構化輸出解析失敗（模型回應解不回 Character）\n");
        }
    } catch (const std::exception& e) {
        std::printf("[real] 結構化輸出失敗：%s\n", e.what());
    }

    // 4) ask_tools 工具呼叫：GetWeather struct 同時生 schema、又解回 arguments
    std::printf("[real] (4/4) ask_tools 工具呼叫……送出中\n");
    try {
        llm::Tool weather = llm::make_tool<GetWeather>("get_weather", "查詢某城市天氣");
        auto calls = llm::ask_tools(client, "東京現在天氣如何？請用攝氏顯示。", { weather });
        std::printf("[real] 模型要求呼叫 %zu 個工具\n", calls.size());
        for (const auto& c : calls) {
            std::printf("[real]   %s(%s)\n", c.name.c_str(), c.arguments.c_str());
            GetWeather args{};
            if (!glz::read_json(args, c.arguments))
                std::printf("[real]   解出參數 => city=%s unit=%s\n", args.city.c_str(), args.unit.c_str());
        }
    } catch (const std::exception& e) {
        std::printf("[real] 工具呼叫失敗：%s\n", e.what());
    }
}

static int run(const std::vector<std::string>& args_in) {
    // ★ --real：另一條路徑，打真後端（本機 LM Studio）——不動離線 fixture 那條回歸路徑，
    //   兩者互斥（給了 --real 就只跑真後端示範，離線 demo 全部略過）。
    //   從 args 裡摘掉這個旗標，不干擾原本 N/名字的位置參數解析。
    std::vector<std::string> args = args_in;
    bool real = false;
    for (auto it = args.begin(); it != args.end(); ) {
        if (*it == "--real") { real = true; it = args.erase(it); }
        else ++it;
    }
    if (real) {
        std::printf("[real] --real 模式：打真後端，略過離線 fixture demo\n");
        demo_real();
        return 0;
    }

    // ★ CLI 模式：只要出現任何 --旗標／-h（--real 已在上面摘掉），就走反射生成的 CLI，
    //   把命令列整包交給 cli::run。沒有旗標時才落到下面的純位置參數 demo（N／名字）。
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (!args[i].empty() && args[i][0] == '-') return cli::run(args);
    }

    // args[0]＝執行檔；args[1..]＝參數。第一個參數當作累加上限 N（預設 10），第二個當名字。
    int n = 10;
    if (args.size() >= 2) {
        n = std::atoi(args[1].c_str());
        if (n <= 0) n = 10;
    }
    std::string name = (args.size() >= 3) ? args[2] : "主人";

    std::string hello = greet(name);   // ← 來自 demo.hpp
    long long total = sum_to(n);       // ← 來自 demo.hpp

    std::printf("%s\n", hello.c_str());
    std::printf("1..%d 的和 = %lld\n", n, total);

    demo_json();          // ── glaze JSON 往返示範
    demo_http();          // ── native HTTP（file://）→ glaze 解碼取台詞
    demo_http_stream();   // ── native HTTP 串流（逐塊回呼）
    demo_llm();           // ── llm::Client：struct＋反射 ask 接口（離線 fixture）
    demo_tool();          // ── llm::ask_tools：工具呼叫（離線 fixture）
    demo_media();         // ── llm::ask_vision：多媒體/vision（離線 fixture）
    demo_structured();    // ── llm::ask_as<T>：結構化輸出（離線 fixture）
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
