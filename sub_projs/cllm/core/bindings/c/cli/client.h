/* client.h — llm_client_t 的欄位設定小工具（C 無動態欄位，補一層依欄位名寫值）。
 *
 * config.c（config 檔那層）與 cli.c（命令列旗標那層）都要「依欄位名把值寫進 llm_client_t」；
 * 這裡集中該邏輯。字串欄位（endpoint/api_key/model）的緩衝由 owned_strings_t 持有、收工一次釋放。 */
#ifndef CLLM_CLI_CLIENT_H
#define CLLM_CLI_CLIENT_H
#include <cllm/cabi.h>
#include "flags.h"

/* 持有三個字串欄位的複本（config／argv 來源生命週期不一，一律複製後自管）。 */
typedef struct {
  char *endpoint, *api_key, *model;
} owned_strings_t;

void owned_free(owned_strings_t *o); /* 釋放三個字串複本 */

/* 依欄位名把字串值寫入 client：F_STR 直接複製；數值型別先 parse，壞值回 -1（呼叫端轉退出碼）。 */
int client_set_from_str(llm_client_t *c, owned_strings_t *o,
                        const char *field, flag_type_t type, const char *val);
/* 依欄位名寫入已 parse 好的數值（config 檔用；含 field_mask 標記）。 */
void client_set_number(llm_client_t *c, const char *field, double val);

#endif /* CLLM_CLI_CLIENT_H */
