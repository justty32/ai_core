// lua_bind.cpp — bind.hpp 的 Lua 那一半：開 Lua VM、註冊 llm.ask（含選項＋串流）、跑 .lua 腳本。
//
// 薄綁定：把 Lua 傳來的東西轉成 AskOpts → 呼叫 bridge_ask（共用引擎）→ 結果塞回 Lua。
// 對照 try_2 的 host.cpp（那條把 JSON/HTTP 也塞進 Lua）；try_4 相反——Lua 只呼 C++ 開的 API。
//
// llm.ask 兩種呼法：
//   llm.ask("你好")                        —— 純字串＝prompt（非串流）
//   llm.ask("你好", "file://…")            —— 第二字串＝endpoint（離線 fixture 用；向下相容）
//   llm.ask{ prompt="你好", temperature=0.7, stream=true, on_delta=function(p) … end, … }
//                                          —— table＝完整選項（對齊 try_2 拍板的 table 進出 API）

#include "bind.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace vmbind {
namespace {

// 從 table（堆疊 idx=1）取一個可選數值欄位；有值（是數字）才寫進 out。
template <class T>
void opt_number_field(lua_State* L, const char* key, std::optional<T>& out) {
    lua_getfield(L, 1, key);
    if (lua_isnumber(L, -1)) out = static_cast<T>(lua_tonumber(L, -1));
    lua_pop(L, 1);
}

// llm.ask(...) → answer 字串。
//
// ★ C++／Lua 邊界的兩個 longjmp 面向都在這裡處理：
//   (a) 回報錯誤：lua_error 是 longjmp，會跳過活著的非平凡 C++ 區域變數解構子——把 std::string
//       關進內層 scope，先複製內容上 Lua 堆疊，scope 解構後才 lua_error（見底下）。
//   (b) 串流回呼：on_delta 會在 client.ask 的 C++ 幀「之內」回呼進 Lua，若腳本函式出錯而 longjmp
//       穿過那些 C++ 幀即 UB——故回呼一律走 **lua_pcall（保護呼叫）**，把錯誤關在 Lua 自己的
//       setjmp 裡，轉成「設 cb_failed＋中止串流」，回到 C++ 正常路徑後再由 (a) 的方式拋出。
int l_llm_ask(lua_State* L) {
    AskOpts opts;
    int cb_ref = LUA_NOREF;   // 串流回呼在 registry 的參照（無回呼＝NOREF）

    if (lua_type(L, 1) == LUA_TTABLE) {
        // ── table 形式：完整選項 ──
        lua_getfield(L, 1, "prompt");
        opts.prompt = luaL_optstring(L, -1, "");
        lua_pop(L, 1);
        lua_getfield(L, 1, "endpoint");
        opts.endpoint = luaL_optstring(L, -1, "");
        lua_pop(L, 1);
        lua_getfield(L, 1, "model");
        if (lua_isstring(L, -1)) opts.model = lua_tostring(L, -1);
        lua_pop(L, 1);
        opt_number_field(L, "temperature", opts.temperature);
        opt_number_field(L, "top_p",       opts.top_p);
        opt_number_field(L, "max_tokens",  opts.max_tokens);
        opt_number_field(L, "seed",        opts.seed);
        lua_getfield(L, 1, "stream");
        opts.stream = lua_toboolean(L, -1);
        lua_pop(L, 1);
        // on_delta：是 function 就存進 registry（luaL_ref 會 pop 該值）
        lua_getfield(L, 1, "on_delta");
        if (lua_isfunction(L, -1)) cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        else                       lua_pop(L, 1);
    } else {
        // ── 簡易形式：llm.ask(prompt [, endpoint]) ──
        opts.prompt   = luaL_checkstring(L, 1);
        opts.endpoint = luaL_optstring(L, 2, "");
    }

    // 串流回呼包成 lambda：用 lua_pcall 保護，錯誤→設 cb_failed＋中止（見上方 (b)）。
    std::string cb_err;
    bool cb_failed = false;
    if (opts.stream && cb_ref != LUA_NOREF) {
        opts.on_delta = [L, cb_ref, &cb_err, &cb_failed](std::string_view piece) -> bool {
            lua_rawgeti(L, LUA_REGISTRYINDEX, cb_ref);          // 推回呼函式
            lua_pushlstring(L, piece.data(), piece.size());     // 推這段 delta
            if (lua_pcall(L, 1, 1, 0) != LUA_OK) {              // 保護呼叫：錯誤不外洩、不 longjmp 穿 C++
                const char* m = lua_tostring(L, -1);
                cb_err = m ? m : "on_delta 回呼出錯";
                lua_pop(L, 1);
                cb_failed = true;
                return true;                                    // 中止串流
            }
            bool abort = lua_toboolean(L, -1);                  // 回呼傳回值：true＝中止
            lua_pop(L, 1);
            return abort;
        };
    }

    bool ok;
    {
        std::string out, err;
        ok = bridge_ask(opts, out, err);
        if (cb_failed) { ok = false; err = cb_err; }   // 回呼出錯覆蓋為失敗
        if (ok) lua_pushlstring(L, out.data(), out.size());
        else    lua_pushlstring(L, err.data(), err.size());
    }  // ← out/err 解構完畢

    if (cb_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, cb_ref);   // 釋放回呼參照
    if (ok) return 1;
    return lua_error(L);   // 用堆疊頂錯誤字串當錯誤物件（longjmp，此刻無活的非平凡 C++ locals）
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

    // 註冊全域 llm 表：目前 ask（含選項＋串流）。之後三擴充（工具／多媒體／結構化）也掛這個表。
    lua_newtable(L);
    lua_pushcfunction(L, l_llm_ask);
    lua_setfield(L, -2, "ask");
    lua_setglobal(L, "llm");

#ifdef TRY4_TRY3_DIR
    // 離線示範便利：FIXTURES = file://<try_3>/test/fixtures/，讓 demo 腳本不寫死機器路徑就能跑假回應。
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
