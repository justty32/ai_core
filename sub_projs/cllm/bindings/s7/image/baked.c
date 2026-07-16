/* baked.c — 把你的 Scheme 邏輯「烤」進一個獨立的 s7 執行檔（免外部 .scm）。
 *
 * s7 沒有 SBCL 那種記憶體映像 dump——它的「image」＝AOT 編成自帶直譯器的 C 執行檔。
 * 這就是產出 s7 image 的方式：s7.c ＋ llm_s7.c（llm-ask）＋你的 scheme 字串，一起編成單一 binary。
 *
 * 編：source ~/repo/dev/env.sh 後
 *   gcc -O2 baked.c ../llm_s7.c "$S7_DIR/s7.c" -I"$S7_DIR" \
 *       $(pkg-config --cflags --libs cllm) -lm -ldl -o cllm-s7-image
 * 跑：./cllm-s7-image 你好 "$CLLM_FIXTURES/fake/chat/completions"
 */
#include <stdio.h>
#include "s7.h"

void llm_s7_init(s7_scheme *sc); /* llm_s7.c：註冊 llm-ask */

/* 烤進去的邏輯（可換成你任意複雜的 scheme；這裡示意一個 main 函式）*/
static const char *BAKED =
    "(define (main prompt ep)"
    "  (display (if (> (string-length ep) 0) (llm-ask prompt ep) (llm-ask prompt)))"
    "  (newline))";

int main(int argc, char **argv) {
  s7_scheme *sc = s7_init();
  llm_s7_init(sc);              /* llm-ask 內建 */
  s7_eval_c_string(sc, BAKED); /* 你的邏輯烤進映像 */
  const char *prompt = argc > 1 ? argv[1] : "你好";
  const char *ep = argc > 2 ? argv[2] : "";
  s7_call(sc, s7_name_to_value(sc, "main"),
          s7_list(sc, 2, s7_make_string(sc, prompt), s7_make_string(sc, ep)));
  return 0;
}
