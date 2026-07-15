// s7_bind.cpp — bind.hpp 的 s7 那一半：開 s7 VM、定義 llm-ask（含選項＋串流）、跑 .scm 腳本。
//
// 薄綁定：把 Scheme 傳來的東西轉成 AskOpts → 呼叫 bridge_ask（共用引擎）→ 結果轉回 s7 物件。
// host 部分對照 try_1 的 s7host.c（綁 *argv*、s7_load 腳本）。
//
// llm-ask 用 s7_define_function_star 定義，故支援位置參數＋:keyword（對齊 try_1 的 :prompt 風格）：
//   (llm-ask "你好")
//   (llm-ask "你好" "file://…")                              ; 第二位置＝endpoint
//   (llm-ask "你好" :temperature 0.7 :stream #t :on-delta (lambda (p) … #f))

#include "bind.hpp"

#include "s7.h"   // 自帶 __cplusplus 守衛＋會 include <complex.h>，故絕不可包 extern "C"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace vmbind {
namespace {

// (llm-ask prompt [endpoint] [:model .. :temperature .. :top-p .. :max-tokens .. :seed .. :stream .. :on-delta ..])
//
// ★ C++／s7 邊界的兩個 longjmp 面向：
//   (a) 回報錯誤：s7_error 是 longjmp——把 std::string 關進內層 scope，訊息先 snprintf 進平凡 char
//       陣列（longjmp 跨越它無妨），scope 解構後才呼 s7_error。
//   (b) 串流回呼：on-delta 在 client.ask 的 C++ 幀「之內」回呼進 s7；用 **s7_call**（它會自帶 catch，
//       腳本端錯誤不會 longjmp 穿過 C++ 幀）。回呼期間把該 procedure GC-protect 住。
s7_pointer s_llm_ask(s7_scheme* sc, s7_pointer args) {
    // s7_define_function_star 會把選項解析成「宣告順序的一般 list」傳進來，逐一取出：
    s7_pointer cur = args;
    auto pop = [&]() -> s7_pointer { s7_pointer v = s7_car(cur); cur = s7_cdr(cur); return v; };
    s7_pointer v_prompt   = pop();
    s7_pointer v_endpoint = pop();
    s7_pointer v_model    = pop();
    s7_pointer v_temp     = pop();
    s7_pointer v_topp     = pop();
    s7_pointer v_maxtok   = pop();
    s7_pointer v_seed     = pop();
    s7_pointer v_stream   = pop();
    s7_pointer v_ondelta  = pop();

    if (!s7_is_string(v_prompt))   // 必填；型別錯在此 longjmp，但此刻無非平凡 C++ locals，安全
        return s7_wrong_type_arg_error(sc, "llm-ask", 1, v_prompt, "a string prompt");

    AskOpts opts;
    opts.prompt = s7_string(v_prompt);
    if (s7_is_string(v_endpoint)) opts.endpoint = s7_string(v_endpoint);
    if (s7_is_string(v_model))    opts.model    = s7_string(v_model);
    if (s7_is_number(v_temp))     opts.temperature = static_cast<float>(s7_number_to_real(sc, v_temp));
    if (s7_is_number(v_topp))     opts.top_p       = static_cast<float>(s7_number_to_real(sc, v_topp));
    if (s7_is_integer(v_maxtok))  opts.max_tokens  = static_cast<int>(s7_integer(v_maxtok));
    if (s7_is_integer(v_seed))    opts.seed        = static_cast<int>(s7_integer(v_seed));
    opts.stream = s7_is_boolean(v_stream) && s7_boolean(sc, v_stream);

    // 串流回呼：包成 lambda，內部用 s7_call（自帶 catch）。回呼期間 GC-protect 該 procedure。
    s7_int cb_loc = -1;
    if (opts.stream && s7_is_procedure(v_ondelta)) {
        s7_pointer cb = v_ondelta;
        cb_loc = s7_gc_protect(sc, cb);
        opts.on_delta = [sc, cb](std::string_view piece) -> bool {
            s7_pointer arg = s7_make_string_with_length(sc, piece.data(), static_cast<s7_int>(piece.size()));
            s7_pointer r = s7_call(sc, cb, s7_list(sc, 1, arg));   // 保護呼叫：錯誤不穿 C++ 幀
            return s7_is_boolean(r) && s7_boolean(sc, r);          // #t＝中止
        };
    }

    char errbuf[512];
    errbuf[0] = '\0';
    {
        std::string out, err;
        if (bridge_ask(opts, out, err)) {
            s7_pointer r = s7_make_string_with_length(sc, out.data(), static_cast<s7_int>(out.size()));
            if (cb_loc >= 0) s7_gc_unprotect_at(sc, cb_loc);
            return r;   // out/err 於本次 return 正常解構（非 longjmp）
        }
        std::snprintf(errbuf, sizeof errbuf, "%s", err.c_str());
    }  // ← out/err 解構完畢
    if (cb_loc >= 0) s7_gc_unprotect_at(sc, cb_loc);
    return s7_error(sc, s7_make_symbol(sc, "llm-error"),
                    s7_list(sc, 1, s7_make_string(sc, errbuf)));
}

// 把 argv[2..]（腳本後面的參數）綁成 *argv*（字串 list，順序保持）——對照 try_1 s7host.c。
void bind_argv(s7_scheme* sc, const std::vector<std::string>& args) {
    s7_pointer lst = s7_nil(sc);
    for (std::size_t i = args.size(); i-- > 2; )        // 由尾往前 cons 才保原順序
        lst = s7_cons(sc, s7_make_string(sc, args[i].c_str()), lst);
    s7_define_variable(sc, "*argv*", lst);
}

}  // namespace

int run_s7(const std::vector<std::string>& args) {
    const std::string& script = args[1];
    s7_scheme* sc = s7_init();
    if (!sc) { std::fputs("try4: s7_init 失敗\n", stderr); return 3; }
    bind_argv(sc, args);

    // define* 風格：位置參數＋:keyword。prompt 無預設（必填、未給則 #f，C 端驗型別）。
    s7_define_function_star(sc, "llm-ask", s_llm_ask,
        "prompt (endpoint \"\") (model #f) (temperature #f) (top-p #f) "
        "(max-tokens #f) (seed #f) (stream #f) (on-delta #f)",
        "(llm-ask prompt [:endpoint :model :temperature :top-p :max-tokens :seed :stream :on-delta]) "
        "問 LLM，回答案字串；stream＝#t 時逐段呼 on-delta（回 #t 可中止）");

#ifdef TRY4_TRY3_DIR
    // 離線示範便利：*fixtures* = file://<try_3>/test/fixtures/（同 Lua 側 FIXTURES 的用意）。
    {
        std::string f = std::string("file://") + TRY4_TRY3_DIR + "/test/fixtures/";
        s7_define_variable(sc, "*fixtures*", s7_make_string(sc, f.c_str()));
    }
#endif

    if (!s7_load(sc, script.c_str())) {
        std::fprintf(stderr, "try4: 找不到或無法載入 s7 腳本：%s\n", script.c_str());
        s7_free(sc);
        return 2;
    }
    s7_free(sc);
    return 0;
}

}  // namespace vmbind
