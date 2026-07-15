// s7_bind.cpp — bind.hpp 的 s7 那一半：開 s7 VM、定義 llm-ask/llm-ask-json/llm-ask-tools、跑 .scm。
//
// 薄綁定：把 Scheme 傳來的東西轉成 AskOpts／ToolSpec → 呼叫 bridge_*（共用引擎）→ 結果轉回 s7。
// **結構化／工具的 JSON 也由 C++ 解**（json_build 把 glaze 解出的樹轉成 s7 原生：物件→alist、陣列→list）。
// host 部分對照 try_1 的 s7host.c（綁 *argv*、s7_load 腳本）。
//
// 定義：
//   (llm-ask prompt [:endpoint :model :temperature :top-p :max-tokens :seed :stream :on-delta]) → 字串
//   (llm-ask-json prompt [:schema :endpoint :name])                                            → alist
//   (llm-ask-tools prompt [:tools :endpoint])  ; tools＝(list (list name desc schema) …)        → (alist …)
//
// ★ longjmp 紀律：s7_error 會跳過活著的非平凡 C++ locals 解構子——把 std::string／vector 關進內層
//   block，錯誤訊息先 snprintf 進平凡 char 陣列，block 解構後才呼 s7_error。串流回呼另用 s7_call
//   （自帶 catch）。建 s7 結構期間 s7_gc_on(sc,false)：中間節點只被 C 區域變數持有，GC 開著會被收。

#include "bind.hpp"

#include "s7.h"   // 自帶 __cplusplus 守衛＋會 include <complex.h>，故絕不可包 extern "C"

#include <glaze/glaze.hpp>   // glz::generic（通用 JSON 樹）＋read_json

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace vmbind {
namespace {

// ── JSON（glz::generic 樹）→ s7 原生值，遞迴。**呼叫端須已 s7_gc_on(sc,false)**（見檔頭）──
s7_pointer json_build(s7_scheme* sc, const glz::generic& v) {
    if (v.is_boolean()) return v.get_boolean() ? s7_t(sc) : s7_f(sc);
    if (v.is_number()) {
        double d = v.get_number();
        if (d == static_cast<double>(static_cast<s7_int>(d)))
            return s7_make_integer(sc, static_cast<s7_int>(d));   // 整值保 integer
        return s7_make_real(sc, d);
    }
    if (v.is_string()) {
        const std::string& s = v.get_string();
        return s7_make_string_with_length(sc, s.data(), static_cast<s7_int>(s.size()));
    }
    if (v.is_array()) {
        const auto& a = v.get_array();
        s7_pointer lst = s7_nil(sc);
        for (std::size_t i = a.size(); i-- > 0; )        // 由尾往前 cons 保順序
            lst = s7_cons(sc, json_build(sc, a[i]), lst);
        return lst;
    }
    if (v.is_object()) {
        const auto& o = v.get_object();
        s7_pointer lst = s7_nil(sc);                     // alist：(("k" . v) …)
        for (const auto& [k, val] : o) {                 // 疊代序的反序（JSON 物件本無序，可接受）
            s7_pointer key = s7_make_string_with_length(sc, k.data(), static_cast<s7_int>(k.size()));
            lst = s7_cons(sc, s7_cons(sc, key, json_build(sc, val)), lst);
        }
        return lst;
    }
    return s7_f(sc);   // null → #f
}

// 單一 JSON 字串 → s7（含 GC 保護包裹）。解不動回 #f。
s7_pointer json_to_s7(s7_scheme* sc, const std::string& json) {
    glz::generic v;
    if (glz::read_json(v, json)) return s7_f(sc);
    s7_gc_on(sc, false);
    s7_pointer r = json_build(sc, v);
    s7_gc_on(sc, true);
    return r;   // r 之後無 s7 配置動作，C 區域變數持有安全
}

// 讀 Scheme 的 tools 引數：(list (list name desc schema) …) → vector<ToolSpec>。純讀，不配置 s7。
std::vector<ToolSpec> read_tools(s7_scheme* /*sc*/, s7_pointer lst) {
    std::vector<ToolSpec> tools;
    for (s7_pointer p = lst; s7_is_pair(p); p = s7_cdr(p)) {
        s7_pointer t = s7_car(p);
        if (!s7_is_pair(t)) continue;
        ToolSpec ts;
        s7_pointer a = t;
        if (s7_is_string(s7_car(a))) ts.name = s7_string(s7_car(a));
        a = s7_cdr(a);
        if (s7_is_pair(a) && s7_is_string(s7_car(a))) ts.description = s7_string(s7_car(a));
        a = s7_cdr(a);
        if (s7_is_pair(a) && s7_is_string(s7_car(a))) ts.schema = s7_string(s7_car(a));
        tools.push_back(std::move(ts));
    }
    return tools;
}

// (llm-ask prompt …) → 答案字串（含選項＋串流；見檔頭）。
s7_pointer s_llm_ask(s7_scheme* sc, s7_pointer args) {
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

    if (!s7_is_string(v_prompt))
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
            return r;
        }
        std::snprintf(errbuf, sizeof errbuf, "%s", err.c_str());
    }
    if (cb_loc >= 0) s7_gc_unprotect_at(sc, cb_loc);
    return s7_error(sc, s7_make_symbol(sc, "llm-error"),
                    s7_list(sc, 1, s7_make_string(sc, errbuf)));
}

// (llm-ask-json prompt :schema :endpoint :name) → 原生 alist（結構化輸出）。
s7_pointer s_llm_ask_json(s7_scheme* sc, s7_pointer args) {
    s7_pointer cur = args;
    auto pop = [&]() -> s7_pointer { s7_pointer v = s7_car(cur); cur = s7_cdr(cur); return v; };
    s7_pointer v_prompt   = pop();
    s7_pointer v_schema   = pop();
    s7_pointer v_endpoint = pop();
    s7_pointer v_name     = pop();

    if (!s7_is_string(v_prompt))
        return s7_wrong_type_arg_error(sc, "llm-ask-json", 1, v_prompt, "a string prompt");

    s7_pointer result = nullptr;
    char errbuf[512];
    errbuf[0] = '\0';
    bool ok;
    {
        std::string prompt   = s7_string(v_prompt);
        std::string schema   = s7_is_string(v_schema)   ? s7_string(v_schema)   : "";
        std::string endpoint = s7_is_string(v_endpoint) ? s7_string(v_endpoint) : "";
        std::string name     = s7_is_string(v_name)     ? s7_string(v_name)     : "response";
        std::string out, err;
        ok = bridge_ask_json(prompt, endpoint, name, schema, out, err);
        if (ok) result = json_to_s7(sc, out);   // out 在此仍活；json_to_s7 讀完即返
        else    std::snprintf(errbuf, sizeof errbuf, "%s", err.c_str());
    }  // std::string 全解構
    if (ok) return result;   // result 之後無 s7 配置，C 變數持有安全
    return s7_error(sc, s7_make_symbol(sc, "llm-error"),
                    s7_list(sc, 1, s7_make_string(sc, errbuf)));
}

// (llm-ask-tools prompt :tools :endpoint) → (alist …)：每個 (("name" . …) ("arguments" . alist))
s7_pointer s_llm_ask_tools(s7_scheme* sc, s7_pointer args) {
    s7_pointer cur = args;
    auto pop = [&]() -> s7_pointer { s7_pointer v = s7_car(cur); cur = s7_cdr(cur); return v; };
    s7_pointer v_prompt   = pop();
    s7_pointer v_tools    = pop();
    s7_pointer v_endpoint = pop();

    if (!s7_is_string(v_prompt))
        return s7_wrong_type_arg_error(sc, "llm-ask-tools", 1, v_prompt, "a string prompt");

    s7_pointer result = nullptr;
    char errbuf[512];
    errbuf[0] = '\0';
    bool ok;
    {
        std::string prompt   = s7_string(v_prompt);
        std::string endpoint = s7_is_string(v_endpoint) ? s7_string(v_endpoint) : "";
        std::vector<ToolSpec>    tools = read_tools(sc, v_tools);
        std::vector<ToolCallOut> calls;
        std::string err;
        ok = bridge_ask_tools(prompt, endpoint, tools, calls, err);
        if (ok) {
            s7_gc_on(sc, false);                          // 整個結果建構期間關 GC
            s7_pointer lst = s7_nil(sc);
            for (std::size_t i = calls.size(); i-- > 0; ) {
                s7_pointer name = s7_make_string(sc, calls[i].name.c_str());
                glz::generic av;
                s7_pointer argv = glz::read_json(av, calls[i].arguments)
                                      ? s7_f(sc) : json_build(sc, av);   // arguments JSON → alist
                s7_pointer entry = s7_list(sc, 2,
                    s7_cons(sc, s7_make_string(sc, "name"), name),
                    s7_cons(sc, s7_make_string(sc, "arguments"), argv));
                lst = s7_cons(sc, entry, lst);
            }
            s7_gc_on(sc, true);
            result = lst;
        } else {
            std::snprintf(errbuf, sizeof errbuf, "%s", err.c_str());
        }
    }
    if (ok) return result;
    return s7_error(sc, s7_make_symbol(sc, "llm-error"),
                    s7_list(sc, 1, s7_make_string(sc, errbuf)));
}

// 讀 Scheme 的 images 引數：每個 entry＝字串（URL）或 list ("url" u)／("file" path [mime])。純讀。
std::vector<ImageSpec> read_images(s7_scheme* /*sc*/, s7_pointer lst) {
    std::vector<ImageSpec> out;
    for (s7_pointer p = lst; s7_is_pair(p); p = s7_cdr(p)) {
        s7_pointer e = s7_car(p);
        ImageSpec im;
        if (s7_is_string(e)) {
            im.url = s7_string(e);
        } else if (s7_is_pair(e) && s7_is_string(s7_car(e))) {
            std::string kind = s7_string(s7_car(e));
            s7_pointer a = s7_cdr(e);
            if (kind == "url" && s7_is_pair(a) && s7_is_string(s7_car(a))) {
                im.url = s7_string(s7_car(a));
            } else if (kind == "file" && s7_is_pair(a) && s7_is_string(s7_car(a))) {
                im.file = s7_string(s7_car(a));
                s7_pointer b = s7_cdr(a);
                if (s7_is_pair(b) && s7_is_string(s7_car(b))) im.mime = s7_string(s7_car(b));
            }
        }
        if (!im.url.empty() || !im.file.empty()) out.push_back(std::move(im));
    }
    return out;
}

// (llm-ask-vision prompt :images :endpoint) → 答案字串。
s7_pointer s_llm_ask_vision(s7_scheme* sc, s7_pointer args) {
    s7_pointer cur = args;
    auto pop = [&]() -> s7_pointer { s7_pointer v = s7_car(cur); cur = s7_cdr(cur); return v; };
    s7_pointer v_prompt   = pop();
    s7_pointer v_images   = pop();
    s7_pointer v_endpoint = pop();

    if (!s7_is_string(v_prompt))
        return s7_wrong_type_arg_error(sc, "llm-ask-vision", 1, v_prompt, "a string prompt");

    char errbuf[512];
    errbuf[0] = '\0';
    {
        std::string prompt   = s7_string(v_prompt);
        std::string endpoint = s7_is_string(v_endpoint) ? s7_string(v_endpoint) : "";
        std::vector<ImageSpec> images = read_images(sc, v_images);
        std::string out, err;
        if (bridge_ask_vision(prompt, endpoint, images, out, err))
            return s7_make_string_with_length(sc, out.data(), static_cast<s7_int>(out.size()));
        std::snprintf(errbuf, sizeof errbuf, "%s", err.c_str());
    }
    return s7_error(sc, s7_make_symbol(sc, "llm-error"),
                    s7_list(sc, 1, s7_make_string(sc, errbuf)));
}

// 把 argv[2..] 綁成 *argv*（字串 list，順序保持）——對照 try_1 s7host.c。
void bind_argv(s7_scheme* sc, const std::vector<std::string>& args) {
    s7_pointer lst = s7_nil(sc);
    for (std::size_t i = args.size(); i-- > 2; )
        lst = s7_cons(sc, s7_make_string(sc, args[i].c_str()), lst);
    s7_define_variable(sc, "*argv*", lst);
}

}  // namespace

int run_s7(const std::vector<std::string>& args) {
    const std::string& script = args[1];
    s7_scheme* sc = s7_init();
    if (!sc) { std::fputs("try4: s7_init 失敗\n", stderr); return 3; }
    bind_argv(sc, args);

    s7_define_function_star(sc, "llm-ask", s_llm_ask,
        "prompt (endpoint \"\") (model #f) (temperature #f) (top-p #f) "
        "(max-tokens #f) (seed #f) (stream #f) (on-delta #f)",
        "(llm-ask prompt […]) 問 LLM，回答案字串；stream＝#t 時逐段呼 on-delta（回 #t 可中止）");
    s7_define_function_star(sc, "llm-ask-json", s_llm_ask_json,
        "prompt (schema \"\") (endpoint \"\") (name \"response\")",
        "(llm-ask-json prompt :schema <JSON Schema 字串> …) 結構化輸出，回原生 alist");
    s7_define_function_star(sc, "llm-ask-tools", s_llm_ask_tools,
        "prompt (tools ()) (endpoint \"\")",
        "(llm-ask-tools prompt :tools (list (list name desc schema) …)) 工具呼叫，回 (alist …)");
    s7_define_function_star(sc, "llm-ask-vision", s_llm_ask_vision,
        "prompt (images ()) (endpoint \"\")",
        "(llm-ask-vision prompt :images (list url (list \"file\" path mime) …)) 多媒體，回答案字串");

#ifdef TRY4_TRY3_DIR
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
