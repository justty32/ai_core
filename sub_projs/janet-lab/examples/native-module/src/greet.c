/* 一個最小的 Janet 原生 (C) 模組。
 * 編 + 測：
 *   cd examples/native-module
 *   jpm build
 *   JANET_PATH=$PWD/build janet -e '(import greet) (print (greet/add 3 4))'
 * 詳解見 docs/10-c-互通.md
 */
#include <janet.h>
#include <string.h>

/* (greet/add a b) -> a + b */
static Janet cfun_add(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 2);                    /* 檢查參數個數 */
    double a = janet_getnumber(argv, 0);        /* 取 + 型別檢查 */
    double b = janet_getnumber(argv, 1);
    return janet_wrap_number(a + b);            /* 包回 Janet 值 */
}

/* (greet/hello name) -> "Hello, <name>!" */
static Janet cfun_hello(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 1);
    const char *name = (const char *)janet_getstring(argv, 0);
    JanetBuffer *b = janet_buffer(0);
    janet_buffer_push_cstring(b, "Hello, ");
    janet_buffer_push_cstring(b, name);
    janet_buffer_push_cstring(b, "!");
    return janet_wrap_string(janet_string(b->data, b->count));
}

/* 函式表：{Janet 名, C 函式, docstring} */
static const JanetReg cfuns[] = {
    {"add",   cfun_add,   "(greet/add a b) 相加"},
    {"hello", cfun_hello, "(greet/hello name) 打招呼"},
    {NULL, NULL, NULL}
};

/* 模組進入點：import 時被呼叫，把 cfuns 註冊進 env */
JANET_MODULE_ENTRY(JanetTable *env) {
    janet_cfuns(env, "greet", cfuns);
}
