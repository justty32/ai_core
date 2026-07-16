// host.cpp — galtxt try_2：內嵌 Lua 5.4 的極簡 C++ host（對應 try_1 的 s7host.c）
//
// s7 內建 main() 只吃「剛好一個檔名」、不把 argv 傳進 Scheme；Lua 亦然（自己寫 host 才有 argv）。
// 本 host 補上：
//   1. 開一個帶標準庫的 Lua VM
//   2. 把命令列參數綁成 Lua 全域 `arg` 表（Lua 慣例）：
//        arg[0]      = 腳本路徑
//        arg[1..n]   = 腳本後面的參數（順序保持）
//        arg[-1]     = host 執行檔本身
//      腳本裡 `#arg`、`arg[i]` 自然可用；cli.lua 由此解析 --flag。
//   3. load 並執行腳本（argv[1]）
//
// Windows：用 wmain + -municode 取寬字元命令列，WideCharToMultiByte 轉 UTF-8，
//          中文參數不亂碼（對齊 s7host.c 的做法）。
//
// 編（Git Bash，先 export PATH=/c/dev/mingw64/bin:$PATH）：
//   g++ -std=c++20 -O2 host.cpp -I vendor/lua -L vendor/lua -llua -o host.exe -municode
//
// 用：host.exe <script.lua> [arg1 arg2 …]
//   無參數 → 印用法、回傳 1；腳本載入/執行失敗 → 印錯誤到 stderr、回傳 2。

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
// 寬字元 → UTF-8
static std::string w2u8(const wchar_t* w) {
    if (!w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(n - 1, '\0');  // n 含結尾 NUL
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
#endif

// 把 argv 綁成 Lua 全域 `arg` 表。args[0] 是 host 執行檔，args[1] 是腳本，其餘是腳本參數。
static void bind_arg_table(lua_State* L, const std::vector<std::string>& args) {
    lua_newtable(L);
    // arg[-1] = host exe
    lua_pushinteger(L, -1);
    lua_pushlstring(L, args[0].data(), args[0].size());
    lua_settable(L, -3);
    // arg[0] = 腳本；arg[1..] = 腳本後面的參數
    for (size_t i = 1; i < args.size(); ++i) {
        lua_pushinteger(L, (lua_Integer)(i - 1));
        lua_pushlstring(L, args[i].data(), args[i].size());
        lua_settable(L, -3);
    }
    lua_setglobal(L, "arg");
}

static int run(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        fputs("用法：host.exe <script.lua> [arg1 arg2 …]\n", stderr);
        return 1;
    }
    lua_State* L = luaL_newstate();
    if (!L) { fputs("host: 無法建立 Lua state\n", stderr); return 2; }
    luaL_openlibs(L);
    bind_arg_table(L, args);

    const std::string& script = args[1];
    if (luaL_loadfile(L, script.c_str()) != LUA_OK) {
        fprintf(stderr, "host: 載入腳本失敗：%s\n", lua_tostring(L, -1));
        lua_close(L);
        return 2;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "host: 執行腳本失敗：%s\n", lua_tostring(L, -1));
        lua_close(L);
        return 2;
    }
    lua_close(L);
    return 0;
}

#ifdef _WIN32
int wmain(int argc, wchar_t** wargv) {
    // 讓主控台用 UTF-8：我們吐的是 UTF-8 位元組，若終端碼頁是 950/936 會被讀成亂碼。
    // 在這裡強制 65001，中文不依賴使用者終端的預設碼頁。stdout 若被導向檔案/管線則無作用（照樣是 UTF-8）。
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
