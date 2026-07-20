/* main.c — 執行檔進入點：裝 SIGINT → 戳取消，把退出碼交還 shell（對齊 cli.cpp 的 main）。
 *
 * 編：cc cli/[a-z]*.c $(pkg-config --cflags --libs cllm jansson) -o cli/llm-c
 * 跑：source ~/dev/cllm/env.sh 後  ./cli/llm-c 你好 --endpoint file://…/fake/chat/completions */
#include "cli.h"
#include <signal.h>

/* SIGINT：只戳取消旗標（llm_ask 在下個安全點乾淨收掉），退出碼 130 由 llm_ask 回 LLM_CANCELLED 定。 */
static void on_sigint(int sig) { (void)sig; cli_request_cancel(); }

int main(int argc, char **argv) {
  signal(SIGINT, on_sigint);
  return cli_main(argc, argv);
}
