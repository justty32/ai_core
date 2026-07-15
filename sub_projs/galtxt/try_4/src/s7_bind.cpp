// s7_bind.cpp — bind.hpp 的 s7 那一半：開 s7 VM、定義 llm-ask、跑 .scm 腳本。
//
// 薄綁定：把 Scheme 傳來的字串轉一轉 → 呼叫 bridge_ask（共用引擎）→ 結果轉回 s7 物件。
// host 部分對照 try_1 的 s7host.c（綁 *argv*、s7_load 腳本）。

#include "bind.hpp"

#include "s7.h"   // 自帶 __cplusplus 守衛＋會 include <complex.h>，故絕不可包 extern "C"

#include <cstdio>
#include <string>
#include <vector>

namespace vmbind {
namespace {

// (llm-ask prompt [endpoint]) → answer 字串。
//
// ★ C++／s7 邊界的錯誤處理：s7_error 同樣是 longjmp，會跳過活著的非平凡 C++ 區域變數解構子。
//   對策＝把 out/err 關進內層 scope；成功路徑在 scope 內就 return（正常解構、非 longjmp），
//   失敗路徑先把訊息 snprintf 進一個**平凡的 char 陣列**（longjmp 跨越它無妨），
//   scope 結束讓 std::string 解構後，才在無活的非平凡 C++ locals 下呼 s7_error。
s7_pointer s_llm_ask(s7_scheme* sc, s7_pointer args) {
    s7_pointer parg = s7_car(args);
    if (!s7_is_string(parg))   // 型別錯：s7_wrong_type_arg_error 會 longjmp，但此刻無 C++ locals，安全
        return s7_wrong_type_arg_error(sc, "llm-ask", 1, parg, "a string prompt");
    const char* prompt = s7_string(parg);

    const char* endpoint = "";
    s7_pointer rest = s7_cdr(args);
    if (s7_is_pair(rest) && s7_is_string(s7_car(rest)))
        endpoint = s7_string(s7_car(rest));

    char errbuf[512];
    errbuf[0] = '\0';
    {
        std::string out, err;
        if (bridge_ask(prompt, endpoint, out, err))
            return s7_make_string_with_length(sc, out.data(), static_cast<s7_int>(out.size()));
        std::snprintf(errbuf, sizeof errbuf, "%s", err.c_str());
    }  // ← out/err 解構完畢；errbuf 是平凡陣列，下面 longjmp 跨越它無妨
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

    // 定義 llm-ask：必填 1（prompt）、選填 1（endpoint）、無 rest。之後三擴充／串流各再 define 一個。
    s7_define_function(sc, "llm-ask", s_llm_ask, 1, 1, false,
        "(llm-ask prompt [endpoint]) 問 LLM，回答案字串；endpoint 空則走 from_env");

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
