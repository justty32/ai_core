/* reqinput.h — 把 CLI 的請求類旗標驗證／組成 llm_request_t 的輸入（對齊 cli.cpp 組請求段）。
 *
 * ⚠ 與 C++ llm 刻意分歧（比照 core-py）：--schema/--tool/--modality 的 cfg 收「字面 JSON 文字」，
 * 不再開檔；解 JSON 失敗＝用法錯。--image/--media 三分流取值在 media.c。 */
#ifndef CLLM_CLI_REQINPUT_H
#define CLLM_CLI_REQINPUT_H
#include <cllm/cabi.h>
#include "argv.h"
#include "internal.h"

/* llm_request_t 的請求輸入四件組（schema／tools／media／modalities）＋ 其 owned 緩衝倉。 */
typedef struct {
  const char *schema_json;       /* NULL＝不用結構化輸出（字面文字，直接嵌入） */
  llm_tool_def_t *tools; size_t tools_count;
  llm_media_in_t *media; size_t media_count;
  llm_modality_t *modalities; size_t modalities_count;
  cli_arena_t arena;             /* 持有 strdup／json_dumps／讀檔的緩衝，reqinput_free 一次釋放 */
} req_inputs_t;

/* 從 parsed_args 組請求輸入。回 EXIT_OK 並填 *r；驗證失敗回退出碼（印 stderr）。 */
int build_request_inputs(const parsed_args_t *p, req_inputs_t *r);
void reqinput_free(req_inputs_t *r);

#endif /* CLLM_CLI_REQINPUT_H */
