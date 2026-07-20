/* output.h — llm_ask 的四個出口回呼打包成 Sink（對齊 core/src/cli_output.cpp 的 Sink）。
 *
 * 把「輸出接線 + 其共享狀態」收成一物：文字吐 stdout、tool_calls 一行一則 JSON、產出媒體落檔
 * --media-out、錯誤吐 stderr。收尾狀態（wrote_text/had_error/media_err）由 cli.c 讀來定退出碼。 */
#ifndef CLLM_CLI_OUTPUT_H
#define CLLM_CLI_OUTPUT_H
#include <cllm/cabi.h>

typedef struct {
  const char *media_out_dir; /* NULL＝沒地方放，收到媒體時明說丟棄 */
  int wrote_text;            /* 有無吐過文字（決定要不要補尾端換行） */
  int had_error;             /* on_error 被呼叫過＝請求真失敗 */
  int media_err;             /* 媒體落檔失敗＝結果真掉了 */
  int media_n;               /* 已落檔媒體數（供編號檔名） */
} sink_t;

/* 把 sink 的四個回呼與其 user 指標裝進 llm_handlers_t。 */
void sink_bind(sink_t *s, llm_handlers_t *h);

#endif /* CLLM_CLI_OUTPUT_H */
