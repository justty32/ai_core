/* config.h — 三層 config 來源解析（對齊 core/src/cli_config.cpp 的 load_into）。
 *
 * 只處理「config 檔」這層（內建預設 → config 檔）；命令列旗標覆寫在 cli.c。
 * config 檔路徑：--config ＞ 環境變數 LLM_CLI_CONFIG ＞ ~/.config/llm/config.json。 */
#ifndef CLLM_CLI_CONFIG_H
#define CLLM_CLI_CONFIG_H
#include <cllm/cabi.h>
#include "client.h"

/* 載入 config 檔進 client。has_config／config_path＝--config 明指。回退出碼（EXIT_OK／EXIT_USAGE）。 */
int load_config(llm_client_t *c, owned_strings_t *o, int has_config, const char *config_path);

#endif /* CLLM_CLI_CONFIG_H */
