/* host.c — 給 s7 binding 用的最小 host：起 s7、註冊 llm-ask、跑腳本或 -e 表達式。
 *
 * 把 s7.c 當庫一起編（*不要* 定 WITH_MAIN，否則撞 s7 自己的 main）：
 *   gcc host.c llm_s7.c $S7_DIR/s7.c -I$S7_DIR -I../../src -L../../build -lcllm -lm -ldl -o llm-s7
 * 用：
 *   ./llm-s7 script.scm [arg1 arg2 …]   # 載入腳本；arg* 綁成 Scheme *argv*（字串 list）
 *   ./llm-s7 -e "(display (llm-ask \"你好\"))"
 * 對齊 galtxt/try_1 的 argv-aware host 慣例。
 */
#include <stdio.h>
#include <string.h>

#include "s7.h"

void llm_s7_init(s7_scheme *sc); /* llm_s7.c */

int main(int argc, char **argv) {
  s7_scheme *sc = s7_init();
  if (!sc) { fprintf(stderr, "s7_init 失敗\n"); return 3; }
  llm_s7_init(sc);

  if (argc >= 3 && strcmp(argv[1], "-e") == 0) {
    s7_eval_c_string(sc, argv[2]);
    return 0;
  }
  if (argc >= 2) {
    /* argv[2..] → *argv*（字串 list，順序保持：從尾往前 cons）*/
    s7_pointer lst = s7_nil(sc);
    for (int i = argc - 1; i >= 2; i--)
      lst = s7_cons(sc, s7_make_string(sc, argv[i]), lst);
    s7_define_variable(sc, "*argv*", lst);
    if (!s7_load(sc, argv[1])) {
      fprintf(stderr, "無法載入腳本: %s\n", argv[1]);
      return 2;
    }
    return 0;
  }
  fprintf(stderr, "用法: %s <script.scm> [args…] | -e \"<expr>\"\n", argv[0]);
  return 1;
}
