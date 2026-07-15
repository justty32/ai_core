// main.cpp — galtxt try_4 進入點：**雙 VM host**（依副檔名跑 .scm／.lua，共用底下 C++ 核心）。
//
// try_4＝三線整合：try_1（s7）、try_2（Lua）、try_3（純 C++）合流。C++ 核心扛重活（借編 try_3），
// s7／Lua 只當薄薄一層吃 C++ 開出來的 function API（綁定見 bind.hpp／s7_bind.cpp／lua_bind.cpp）。
//
// 分派（見 run()）：
//   try4.exe scripts/demo.scm [參數...]   → s7  host（vmbind::run_s7）
//   try4.exe scripts/demo.lua [參數...]   → Lua host（vmbind::run_lua）
//   try4.exe（無參數）／--smoke           → 煙霧測試（sanity：核心四能力＋兩 VM 都活著）
//
// 本檔自己還留著 smoke_*（核心／Lua／s7 各起一次）當 sanity；真正的腳本執行與 llm 綁定在
// bind 那三個 TU。離線 fixture 借 try_3 的（TRY4_TRY3_DIR 由 CMake 注入指向 ../try_3）。
// 跨平台：Windows 用 wmain + -municode + SetConsoleOutputCP(CP_UTF8)（中文不亂碼），
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
#include "bind.hpp"          // vmbind::run_lua / run_s7：把 C++ 綁給腳本 VM、跑 .lua/.scm

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

// ── 無腳本參數時跑的煙霧測試：證核心＋兩 VM 都活著（sanity）──────────────────
static int run_smoke() {
    std::printf("== galtxt try_4：三線整合（try_3 C++ 核心當底，s7＋Lua 薄層）==\n");
    std::printf("[core] 借編 try_3/src 的核心，從 try_4 自己的 main 呼叫、跑離線 fixture：\n");
    smoke_core();

    std::printf("[vm ] 兩個腳本 VM 與 C++ 核心共存於同一顆 exe：\n");
    smoke_lua();
    smoke_s7();

    llm::Client real = llm::from_env();
    std::printf("[core] from_env() 真後端 endpoint＝%s（本步不實打，僅證明連結）\n",
                real.endpoint.c_str());
    std::printf("== sanity 結束。給腳本即跑：try4.exe scripts/demo.scm｜scripts/demo.lua ==\n");
    return 0;
}

// 副檔名判斷（小工具）：s 是否以 suffix 結尾。
static bool ends_with(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static int run(const std::vector<std::string>& args) {
    // 無腳本參數 → 跑煙霧測試（sanity）；或顯式 --smoke。
    if (args.size() < 2 || args[1] == "--smoke") return run_smoke();

    // 有腳本參數 → 依副檔名分派到對應 VM host（.scm→s7、.lua→Lua）。
    // 這就是 try_4 當「雙 VM host」的樣子：一顆 exe、兩種薄腳本語言，共用底下同一套 C++ 核心。
    const std::string& script = args[1];
    if (ends_with(script, ".scm")) return vmbind::run_s7(args);
    if (ends_with(script, ".lua")) return vmbind::run_lua(args);

    std::fprintf(stderr,
        "用法：try4.exe <腳本.scm|腳本.lua> [參數...]｜try4.exe --smoke\n"
        "  未知副檔名：%s（只認 .scm／.lua）\n", script.c_str());
    return 1;
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
