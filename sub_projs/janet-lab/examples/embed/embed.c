/* 把 Janet 嵌進 C 程式。
 * 編 + 跑（libjanet.a 靜態連入，產物不需系統有 janet）：
 *   cc embed.c -I$HOME/.local/include/janet $HOME/.local/lib/libjanet.a \
 *      -lm -ldl -lpthread -lrt -rdynamic -o embed
 *   ./embed
 * 詳解見 docs/10-c-互通.md
 */
#include <janet.h>
#include <stdio.h>

int main(void) {
    janet_init();                              /* 起 Janet runtime */
    JanetTable *env = janet_core_env(NULL);    /* 拿到核心環境 */

    /* 1) 直接跑一段 Janet 原始碼 */
    janet_dostring(env, "(print \"從 C 裡跑 Janet：\" (+ 2 3))", "embed", NULL);

    /* 2) 求值一個運算式，把結果撈回 C */
    Janet out;
    janet_dostring(env, "(* 6 7)", "embed", &out);
    printf("C 收到的結果 = %d\n", (int)janet_unwrap_number(out));

    janet_deinit();
    return 0;
}
