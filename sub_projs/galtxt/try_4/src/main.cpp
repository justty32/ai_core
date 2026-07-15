// main.cpp — galtxt try_4 進入點（第一里程碑：只含 C++ 核心的煙霧測試）。
//
// try_4＝三線整合：try_1（s7）、try_2（Lua）、try_3（純 C++）合流。C++ 核心扛重活，
// s7／Lua 之後只當薄薄一層吃 C++ 開出來的 function API。本檔是整合的**第一步**：
// 先證明「借編 try_3 核心」這條路走得通——用 try_4 自己的 main（非 try_3 的 main），
// include try_3/src 的介面、呼叫其 llm::Client 及三擴充，跑通離線 fixture。
//
//   ✅ 這一步證明的事：try_3 的核心 .cpp（http/llm/llm_tool/llm_media/llm_json）能被
//      try_4 的 CMakeLists「借編」進來、與新 main 連結成 try4.exe、行為與在 try_3 裡一致。
//   ⏳ 還沒做的事（下一步）：嵌入 s7／Lua、把下面這些 C++ 呼叫綁成腳本可呼叫的 API，
//      讓 .scm／.lua 薄腳本也能發問。跨語言邊界（腳本函式當 C++ std::function 的串流回呼）
//      是最需要試水的一段。
//
// 離線 fixture 直接借 try_3 的（TRY4_TRY3_DIR 由 CMake 注入指向 ../try_3），不另抄假回應。
// 跨平台：Windows 用 wmain + -municode + SetConsoleOutputCP(CP_UTF8)（中文 argv／輸出不亂碼），
//         Linux/Mac 走標準 main、原生 UTF-8。對齊 try_3 main.cpp。

#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>   // 反射式 JSON↔struct（借核心也需要它解回 arguments）

#include "llm.hpp"           // llm::Client：struct＋反射 ask 接口（借自 try_3/src）
#include "llm_tool.hpp"      // llm::make_tool / ask_tools：工具呼叫
#include "llm_media.hpp"     // llm::image_from_url / ask_vision：多媒體/vision
#include "llm_json.hpp"      // llm::ask_as<T>：結構化輸出

// 兩個 VM 的標頭（借編來源見 CMakeLists (B)(C)）：
//   Lua 標頭不自帶 C++ 連結標記，須自行 extern "C"（對齊 try_2 host.cpp）。
//   s7.h **自帶** __cplusplus 守衛，且它會 include <complex.h>（C++ 下映射成 <complex>、內有
//   literal operator ""i 等）——那些**不能**放進 extern "C"，故 s7.h 絕不可自己再包一層，直接 include。
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include "s7.h"

#ifdef _WIN32
#include <windows.h>
static std::string w2u8(const wchar_t* w) {
    if (!w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
#endif

// 結構化輸出的目標型別＝唯一真相源（反射生 schema 約束模型、回來的 JSON 再反射回它）。
struct Character {
    std::string name;
    int affection{};
    std::vector<std::string> lines;
};

// 工具參數型別＝唯一真相源（make_tool 反射生 schema、又解回模型產生的 arguments）。
struct GetWeather {
    std::string city;   // 城市名
    std::string unit;   // 溫度單位：celsius / fahrenheit
};

#ifdef TRY4_TRY3_DIR
// 借 try_3 的離線 fixture：拼一個指向其 test/fixtures/<名> 的 file:// URL。
static std::string fixture_url(std::string_view name) {
    return std::string("file://") + TRY4_TRY3_DIR + "/test/fixtures/" + std::string(name)
           + "/chat/completions";
}
#endif

// ── 煙霧測試：用「借編的核心」跑四種能力各一次（全離線 fixture）──────────────
static void smoke_core() {
#ifdef TRY4_TRY3_DIR
    // (1) ask 非串流：組請求→送→反射解回應→取答覆
    try {
        llm::Client client{ .endpoint = fixture_url("fake") };
        std::printf("[core] (1/4) ask 非串流 => %s\n", client.ask("你好").c_str());
    } catch (const std::exception& e) {
        std::printf("[core] (1/4) 失敗：%s\n", e.what());
    }

    // (2) ask 串流：SSE 逐段經 on_delta 餵出（回呼回 true＝中止；這裡永遠 false＝收完）
    try {
        llm::Client streamer{ .endpoint = fixture_url("fake_stream") };
        std::printf("[core] (2/4) ask 串流 => ");
        std::string whole = streamer.ask("你好", true, [](std::string_view piece) {
            std::printf("[%.*s]", static_cast<int>(piece.size()), piece.data());
            return false;
        });
        std::printf("　合＝%s\n", whole.c_str());
    } catch (const std::exception& e) {
        std::printf("[core] (2/4) 失敗：%s\n", e.what());
    }

    // (3) ask_as<T>：丟 Character 進、拿 Character 出（結構化輸出）
    try {
        llm::Client client{ .endpoint = fixture_url("fake_json") };
        if (auto c = llm::ask_as<Character>(client, "生成一個傲嬌女角色", "character")) {
            std::printf("[core] (3/4) 結構化 => name=%s affection=%d lines[0]=%s\n",
                        c->name.c_str(), c->affection, c->lines.empty() ? "" : c->lines[0].c_str());
        } else {
            std::printf("[core] (3/4) 結構化解析失敗\n");
        }
    } catch (const std::exception& e) {
        std::printf("[core] (3/4) 失敗：%s\n", e.what());
    }

    // (4) ask_tools：GetWeather 同時生 schema、又解回 arguments（工具呼叫）
    try {
        llm::Client client{ .endpoint = fixture_url("fake_tool") };
        llm::Tool weather = llm::make_tool<GetWeather>("get_weather", "查詢某城市天氣");
        auto calls = llm::ask_tools(client, "東京天氣如何？", { weather });
        std::printf("[core] (4/4) 工具呼叫 => 模型要求 %zu 個工具\n", calls.size());
        for (const auto& c : calls) {
            GetWeather args{};
            if (!glz::read_json(args, c.arguments))
                std::printf("[core]        %s → city=%s unit=%s\n",
                            c.name.c_str(), args.city.c_str(), args.unit.c_str());
        }
    } catch (const std::exception& e) {
        std::printf("[core] (4/4) 失敗：%s\n", e.what());
    }
#else
    std::printf("[core] 未注入 TRY4_TRY3_DIR，跳過離線煙霧測試\n");
#endif
}

// ── 煙霧測試：Lua VM 在同一顆 exe 裡活著、能跑、中文不亂碼 ──────────────────
//    這一步只證明「VM 起得來、eval 得動」；把 C++ 的 llm::ask 綁成 Lua 可呼叫的
//    function 是下一步（lua_pushcfunction 綁定層）。
static void smoke_lua() {
    lua_State* L = luaL_newstate();
    if (!L) { std::printf("[lua] 無法建立 lua_State\n"); return; }
    luaL_openlibs(L);   // ← 靠 src/lua_linit_clean.c 提供的 luaL_openselectedlibs（乾淨版）

    // eval 一段 Lua：算術＋中文字串，回傳到 C++ 這側讀出來（證明跨語言值傳遞通）
    if (luaL_dostring(L, "return 6 * 7, 'Lua 說：你好'") == LUA_OK) {
        lua_Integer n = lua_tointeger(L, -2);
        const char* s = lua_tostring(L, -1);
        std::printf("[lua] VM 起動 => 6*7=%lld，%s\n", static_cast<long long>(n), s ? s : "");
    } else {
        std::printf("[lua] eval 失敗：%s\n", lua_tostring(L, -1));
    }
    lua_close(L);
}

// ── 煙霧測試：s7 Scheme VM 在同一顆 exe 裡活著、能 eval、中文不亂碼 ────────────
//    同樣只證明 VM 通；s7_define_function 把 C++ 綁成 Scheme 可呼叫的過程，下一步做。
static void smoke_s7() {
    s7_scheme* sc = s7_init();
    if (!sc) { std::printf("[s7] s7_init 失敗\n"); return; }

    s7_pointer sum = s7_eval_c_string(sc, "(+ 1 2 3)");
    s7_pointer greet = s7_eval_c_string(sc, "(string-append \"s7 說：\" \"你好\")");
    std::printf("[s7]  VM 起動 => (+ 1 2 3)=%lld，%s\n",
                static_cast<long long>(s7_integer(sum)), s7_string(greet));
    s7_free(sc);
}

static int run(const std::vector<std::string>& /*args*/) {
    std::printf("== galtxt try_4：三線整合（try_3 C++ 核心當底，s7＋Lua 薄層待接）==\n");
    std::printf("[core] 借編 try_3/src 的核心，從 try_4 自己的 main 呼叫、跑離線 fixture：\n");
    smoke_core();

    std::printf("[vm ] 兩個腳本 VM 與 C++ 核心共存於同一顆 exe（本步只證 VM 通，綁定層下一步）：\n");
    smoke_lua();
    smoke_s7();

    // from_env() 也是借來的核心：印出它組出的真後端 endpoint，證明工廠函式連結無誤
    //（不實際打真後端，那要另開 --real，留待核心煙霧穩定後再加）。
    llm::Client real = llm::from_env();
    std::printf("[core] from_env() 真後端 endpoint＝%s（本步不實打，僅證明連結）\n",
                real.endpoint.c_str());
    std::printf("== 核心煙霧測試結束：借編路徑通，下一步接 s7／Lua 綁定 ==\n");
    return 0;
}

#ifdef _WIN32
int wmain(int argc, wchar_t** wargv) {
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
