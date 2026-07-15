// lua_bind.cpp — bind.hpp 的 Lua 那一半：開 Lua VM、註冊 llm.{ask,ask_json,ask_tools}、跑 .lua 腳本。
//
// 薄綁定：把 Lua 傳來的東西轉成 AskOpts／ToolSpec → 呼叫 bridge_*（共用引擎）→ 結果塞回 Lua。
// 對照 try_2 的 host.cpp（那條把 JSON/HTTP 也塞進 Lua）；try_4 相反——Lua 只呼 C++ 開的 API，
// **連結構化／工具的 JSON 都由 C++ 解**（push_json 把 glaze 解出的樹轉成原生 Lua table）。
//
// llm 表：
//   llm.ask("你好")／llm.ask{prompt=…, temperature=…, stream=…, on_delta=…}     → 答案字串
//   llm.ask_json{prompt=…, schema=<JSON Schema 字串>, endpoint=…, name=…}        → 原生 table
//   llm.ask_tools{prompt=…, endpoint=…, tools={ {name=,description=,schema=}… }}  → {{name=,arguments=table}…}
//
// ★ longjmp 紀律（lua_error 會跳過活著的非平凡 C++ 區域變數解構子）：每個綁定函式把**所有
//   非平凡 C++ locals 關進一個內層 block**，先把結果／錯誤訊息複製上 Lua 堆疊，block 結束解構後，
//   才在「只剩平凡 locals」時呼 lua_error。回呼另用 lua_pcall 保護（見 l_llm_ask）。

#include "bind.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <glaze/glaze.hpp>   // glz::generic（通用 JSON 樹）＋read_json

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace vmbind {
namespace {

// ── JSON（glz::generic 樹）→ 原生 Lua 值，遞迴推一個值到堆疊頂 ────────────────
//    Lua 的 GC 不會回收「堆疊上或已被 table 參照」的物件，故建構期天然安全，不需額外保護
//    （與 s7 側要 s7_gc_on(false) 不同）。
void push_json(lua_State* L, const glz::generic& v) {
    if (v.is_boolean()) {
        lua_pushboolean(L, v.get_boolean());
    } else if (v.is_number()) {
        double d = v.get_number();
        if (d == static_cast<double>(static_cast<lua_Integer>(d)))
            lua_pushinteger(L, static_cast<lua_Integer>(d));   // 整值保 integer 子型別（Lua 5.5）
        else
            lua_pushnumber(L, d);
    } else if (v.is_string()) {
        const std::string& s = v.get_string();
        lua_pushlstring(L, s.data(), s.size());
    } else if (v.is_array()) {
        const auto& a = v.get_array();
        lua_createtable(L, static_cast<int>(a.size()), 0);
        for (std::size_t i = 0; i < a.size(); ++i) {
            push_json(L, a[i]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));   // 1-based 陣列
        }
    } else if (v.is_object()) {
        const auto& o = v.get_object();
        lua_createtable(L, 0, static_cast<int>(o.size()));
        for (const auto& [k, val] : o) {
            push_json(L, val);
            lua_setfield(L, -2, k.c_str());
        }
    } else {
        lua_pushnil(L);   // null 或未知 → nil
    }
}

// 從 table（堆疊 idx=1）取一個可選數值欄位；有值（是數字）才寫進 out。
template <class T>
void opt_number_field(lua_State* L, const char* key, std::optional<T>& out) {
    lua_getfield(L, 1, key);
    if (lua_isnumber(L, -1)) out = static_cast<T>(lua_tonumber(L, -1));
    lua_pop(L, 1);
}

// llm.ask(...) → answer 字串（含選項＋串流；見檔頭）。
int l_llm_ask(lua_State* L) {
    int cb_ref = LUA_NOREF;   // 串流回呼在 registry 的參照（平凡，可存活過下方 block）
    bool ok;
    {
        AskOpts opts;
        if (lua_type(L, 1) == LUA_TTABLE) {
            lua_getfield(L, 1, "prompt");   opts.prompt   = luaL_optstring(L, -1, ""); lua_pop(L, 1);
            lua_getfield(L, 1, "endpoint"); opts.endpoint = luaL_optstring(L, -1, ""); lua_pop(L, 1);
            lua_getfield(L, 1, "model");
            if (lua_isstring(L, -1)) opts.model = lua_tostring(L, -1);
            lua_pop(L, 1);
            opt_number_field(L, "temperature", opts.temperature);
            opt_number_field(L, "top_p",       opts.top_p);
            opt_number_field(L, "max_tokens",  opts.max_tokens);
            opt_number_field(L, "seed",        opts.seed);
            lua_getfield(L, 1, "stream"); opts.stream = lua_toboolean(L, -1); lua_pop(L, 1);
            lua_getfield(L, 1, "on_delta");
            if (lua_isfunction(L, -1)) cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            else                       lua_pop(L, 1);
        } else {
            opts.prompt   = luaL_checkstring(L, 1);
            opts.endpoint = luaL_optstring(L, 2, "");
        }

        std::string cb_err;
        bool cb_failed = false;
        if (opts.stream && cb_ref != LUA_NOREF) {
            // 串流回呼：lua_pcall 保護，錯誤→設 cb_failed＋中止（不 longjmp 穿 client.ask 的 C++ 幀）。
            opts.on_delta = [L, cb_ref, &cb_err, &cb_failed](std::string_view piece) -> bool {
                lua_rawgeti(L, LUA_REGISTRYINDEX, cb_ref);
                lua_pushlstring(L, piece.data(), piece.size());
                if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
                    const char* m = lua_tostring(L, -1);
                    cb_err = m ? m : "on_delta 回呼出錯";
                    lua_pop(L, 1);
                    cb_failed = true;
                    return true;
                }
                bool abort = lua_toboolean(L, -1);
                lua_pop(L, 1);
                return abort;
            };
        }

        std::string out, err;
        ok = bridge_ask(opts, out, err);
        if (cb_failed) { ok = false; err = cb_err; }
        if (ok) lua_pushlstring(L, out.data(), out.size());
        else    lua_pushlstring(L, err.data(), err.size());
    }  // ← opts／cb_err／out／err 全解構
    if (cb_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, cb_ref);
    if (ok) return 1;
    return lua_error(L);
}

// llm.ask_json{prompt=, schema=, endpoint=, name=} → 原生 table（結構化輸出）。
int l_llm_ask_json(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    bool ok;
    {
        std::string prompt, schema, endpoint, name, err;
        lua_getfield(L, 1, "prompt");   prompt   = luaL_optstring(L, -1, "");         lua_pop(L, 1);
        lua_getfield(L, 1, "schema");   schema   = luaL_optstring(L, -1, "");         lua_pop(L, 1);
        lua_getfield(L, 1, "endpoint"); endpoint = luaL_optstring(L, -1, "");         lua_pop(L, 1);
        lua_getfield(L, 1, "name");     name     = luaL_optstring(L, -1, "response"); lua_pop(L, 1);

        std::string out;
        ok = bridge_ask_json(prompt, endpoint, name, schema, out, err);
        if (ok) {
            glz::generic v;
            if (glz::read_json(v, out)) { ok = false; err = "結構化回應不是合法 JSON"; }
            else push_json(L, v);   // 原生 table 推到堆疊頂
        }
        if (!ok) lua_pushlstring(L, err.data(), err.size());
    }
    if (ok) return 1;
    return lua_error(L);
}

// llm.ask_tools{prompt=, endpoint=, tools={{name=,description=,schema=}…}} → {{name=,arguments=table}…}
int l_llm_ask_tools(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    bool ok;
    {
        std::string prompt, endpoint, err;
        lua_getfield(L, 1, "prompt");   prompt   = luaL_optstring(L, -1, ""); lua_pop(L, 1);
        lua_getfield(L, 1, "endpoint"); endpoint = luaL_optstring(L, -1, ""); lua_pop(L, 1);

        std::vector<ToolSpec> tools;
        lua_getfield(L, 1, "tools");
        if (lua_istable(L, -1)) {
            lua_Integer n = static_cast<lua_Integer>(lua_rawlen(L, -1));
            for (lua_Integer i = 1; i <= n; ++i) {
                lua_rawgeti(L, -1, i);
                if (lua_istable(L, -1)) {
                    ToolSpec ts;
                    lua_getfield(L, -1, "name");        ts.name        = luaL_optstring(L, -1, ""); lua_pop(L, 1);
                    lua_getfield(L, -1, "description");  ts.description = luaL_optstring(L, -1, ""); lua_pop(L, 1);
                    lua_getfield(L, -1, "schema");       ts.schema      = luaL_optstring(L, -1, ""); lua_pop(L, 1);
                    tools.push_back(std::move(ts));
                }
                lua_pop(L, 1);   // pop tools[i]
            }
        }
        lua_pop(L, 1);           // pop tools

        std::vector<ToolCallOut> calls;
        ok = bridge_ask_tools(prompt, endpoint, tools, calls, err);
        if (ok) {
            lua_createtable(L, static_cast<int>(calls.size()), 0);
            for (std::size_t i = 0; i < calls.size(); ++i) {
                lua_createtable(L, 0, 2);
                lua_pushlstring(L, calls[i].name.data(), calls[i].name.size());
                lua_setfield(L, -2, "name");
                glz::generic v;
                if (!glz::read_json(v, calls[i].arguments)) push_json(L, v);   // arguments JSON → 原生 table
                else lua_pushlstring(L, calls[i].arguments.data(), calls[i].arguments.size());  // 解不動就給原字串
                lua_setfield(L, -2, "arguments");
                lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
            }
        } else {
            lua_pushlstring(L, err.data(), err.size());
        }
    }
    if (ok) return 1;
    return lua_error(L);
}

// 把 argv 綁成 Lua 全域 arg 表（Lua 慣例，對齊 try_2 host.cpp）。
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

    // 全域 llm 表：ask（含選項＋串流）＋ ask_json（結構化）＋ ask_tools（工具呼叫）。
    lua_newtable(L);
    lua_pushcfunction(L, l_llm_ask);       lua_setfield(L, -2, "ask");
    lua_pushcfunction(L, l_llm_ask_json);  lua_setfield(L, -2, "ask_json");
    lua_pushcfunction(L, l_llm_ask_tools); lua_setfield(L, -2, "ask_tools");
    lua_setglobal(L, "llm");

#ifdef TRY4_TRY3_DIR
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
