/* media.h — --image/--media 的 MIME 對照與三分流取值（對齊 core-py 的 media.py）。
 *
 * mime_of／ext_of 對齊 cli_config.cpp 的同名對照表。build_media_item 是三分流取值：除了讀二進位
 * 圖檔，也吃「已編碼」形式（data:/http URL、或 .json 描述子），省掉每次重算 base64。 */
#ifndef CLLM_CLI_MEDIA_H
#define CLLM_CLI_MEDIA_H
#include <cllm/cabi.h>
#include "internal.h"

const char *mime_of(const char *path); /* 副檔名 → MIME */
const char *ext_of(const char *mime);  /* MIME → 落檔副檔名 */

/* 一個 --image/--media 值 → 填 *out（llm_media_in_t）。owned 緩衝登記進 arena。
 * 成功回 0；失敗印 stderr 回 -1。 */
int build_media_item(const char *spec, llm_media_in_t *out, cli_arena_t *arena);

#endif /* CLLM_CLI_MEDIA_H */
