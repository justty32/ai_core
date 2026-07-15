// lua_bind.cpp — bind.hpp 的 Lua 那一半：開 Lua VM、註冊 llm.ask、跑 .lua 腳本。
//
// 薄綁定：把 Lua 傳來的字串轉一轉 → 呼叫 bridge_ask（共用引擎）→ 結果塞回 Lua。
// 對照 try_2 的 host.cpp（那條把 JSON/HTTP 也塞進 Lua）；try_4 相反——Lua 只呼 C++ 開的 API。

#include "bind.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cstdio>
#include <string>
#include <vector>

namespace vmbind {
namespace {

// llm.ask(prompt [, endpoint]) → answer 字串。
//
// ★ C++／Lua 邊界的錯誤處理：lua_error 是 longjmp，會跳過還活著的**非平凡 C++ 區域變數**的解構子。
//   對策＝把 out/err 關進一個內層 scope：先把要用的內容複製上 Lua 堆疊（Lua 自行管理那份複本），
//   scope 結束時 std::string 正常解構，**之後**才呼 lua_error——longjmp 當下已無活的非平凡 C++ locals。
int l_llm_ask(lua_State* L) {
    const char* prompt   = luaL_checkstring(L, 1);       // 非字串會在此 longjmp，但此刻無 C++ locals，安全
    const char* endpoint = luaL_optstring(L, 2, "");     // 選填；空＝走 from_env
    bool ok;
    {
        std::string out, err;
        ok = bridge_ask(prompt, endpoint, out, err);
        // 成功推答案、失敗推錯誤訊息——都趁 out/err 還在，複製進 Lua（lua_pushlstring 會複製）
        if (ok) lua_pushlstring(L, out.data(), out.size());
        else    lua_pushlstring(L, err.data(), err.size());
    }  // ← out/err 在這裡解構完畢；下面若 longjmp 也不會跳過它們
    if (ok) return 1;              // 答案在堆疊頂
    return lua_error(L);           // 用堆疊頂的錯誤字串當錯誤物件丟出（longjmp，此刻無活的非平凡 C++ locals）
}

// 把 argv 綁成 Lua 全域 arg 表（Lua 慣例，對齊 try_2 host.cpp）：
//   arg[-1]=exe、arg[0]=腳本、arg[1..]=腳本後面的參數。
void bind_arg(lua_State* L, const std::vector<std::string>& args) {
    lua_newtable(L);
    lua_pushinteger(L, -1);
    lua_pushlstring(L, args[0].data(), args[0].size());
    lua_settable(L, -3);
    for (std::size_t i = 1; i < args.size(); ++i) {
        lua_pushinteger(L, static_cast<lua_Integer>(i - 1));
        lua_pushlstring(L, args[i].data(), args[i].size());
        lua_settable(L, -3);
    }
    lua_setglobal(L, "arg");
}

}  // namespace

int run_lua(const std::vector<std::string>& args) {
    const std::string& script = args[1];
    lua_State* L = luaL_newstate();
    if (!L) { std::fputs("try4: 無法建立 Lua state\n", stderr); return 2; }
    luaL_openlibs(L);              // ← 靠 lua_linit_clean.c 的 luaL_openselectedlibs（原味版）
    bind_arg(L, args);

    // 註冊全域 llm 表：目前只有 ask。之後三擴充（工具／多媒體／結構化）＋串流都掛在這個表上。
    lua_newtable(L);
    lua_pushcfunction(L, l_llm_ask);
    lua_setfield(L, -2, "ask");
    lua_setglobal(L, "llm");

#ifdef TRY4_TRY3_DIR
    // 離線示範便利：FIXTURES = file://<try_3>/test/fixtures/，讓 demo 腳本不寫死機器路徑就能跑假回應。
    //（真用途可略：不傳 endpoint 走 from_env，或腳本自帶真 endpoint。）
    {
        std::string f = std::string("file://") + TRY4_TRY3_DIR + "/test/fixtures/";
        lua_pushlstring(L, f.data(), f.size());
        lua_setglobal(L, "FIXTURES");
    }
#endif

    if (luaL_loadfile(L, script.c_str()) != LUA_OK) {
        std::fprintf(stderr, "try4: 載入 Lua 腳本失敗：%s\n", lua_tostring(L, -1));
        lua_close(L);
        return 2;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        std::fprintf(stderr, "try4: 執行 Lua 腳本失敗：%s\n", lua_tostring(L, -1));
        lua_close(L);
        return 2;
    }
    lua_close(L);
    return 0;
}

}  // namespace vmbind
