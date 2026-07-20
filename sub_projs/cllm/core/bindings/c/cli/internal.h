/* internal.h — CLI 共用的退出碼、環境變數鍵、檔案讀取小工具（對齊 core/src/cli_internal.hpp）。
 *
 * 葉模組：只依標準庫、不依賴其餘 CLI 模組，作為 flags/config/media/output/cli 的共用底，
 * 把依賴收成一張無環 DAG。 */
#ifndef CLLM_CLI_INTERNAL_H
#define CLLM_CLI_INTERNAL_H
#include <stddef.h>

/* 退出碼（對齊 cli_internal.hpp）：0 成功；1 用法錯；2 請求失敗；130 SIGINT 取消。 */
#define EXIT_OK 0
#define EXIT_USAGE 1
#define EXIT_REQUEST 2
#define EXIT_CANCEL 130

#define CONFIG_ENV_VAR "LLM_CLI_CONFIG" /* 對齊 cli_internal.hpp 的 kConfigEnvVar */

/* 整檔讀成 raw bytes（媒體圖檔等）。成功回 malloc 的緩衝並填 *len；讀不到回 NULL。 */
char *cli_read_file_bytes(const char *path, size_t *len);
/* 整檔讀成 NUL 結尾文字（config／media 描述子等）。成功回 malloc 字串；讀不到回 NULL。 */
char *cli_read_file_text(const char *path);

/* 指標小倉：把散落的 malloc 緩衝集中登記，收工一次釋放（media／reqinput 共用）。 */
typedef struct {
  void **items;
  size_t n, cap;
} cli_arena_t;

void *cli_arena_take(cli_arena_t *a, void *p); /* 登記 p、原樣回傳（配置失敗時回 NULL） */
void cli_arena_free(cli_arena_t *a);           /* 釋放所有登記緩衝 ＋ items 本身 */

#endif /* CLLM_CLI_INTERNAL_H */
