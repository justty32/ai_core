#ifndef LLM_LOGIN_ABI_H
#define LLM_LOGIN_ABI_H
/* login_abi.h — llm-login 對外 C ABI（純 C 可 include，仿 cllm 的 cabi.h 樣式）。
 *
 * 用途＝與 cllm 聯動：當 cllm 連線出錯（api 失效／沒登入）時，harness 把 opts 交給
 * llm_login_login，它自動開瀏覽器→接 callback→換 token→patch cllm config.json，
 * 之後 harness 重試 cllm 即可。cllm 核心一行不動。
 *
 * 記憶體約定（同 cabi.h）：本 ABI 不配置任何要呼叫端釋放的東西——字串走呼叫端提供的 buffer。
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  LLM_LOGIN_OK = 0,
  LLM_LOGIN_ERR = 1,        /* 一般錯誤（訊息在 opts->err）*/
  LLM_LOGIN_NEED_LOGIN = 2  /* 沒憑證/過期且無法 refresh → 要重跑 login */
} llm_login_status_t;

/* 三個路徑任一為 NULL/"" → 用預設探測（env 或 ~/.config/llm/…）。 */
typedef struct {
  const char *provider_path; /* oauth.json（provider 設定）；預設 env LLM_OAUTH_PROVIDER 或 ~/.config/llm/oauth.json */
  const char *token_path;    /* oauth_token.json；預設 ~/.config/llm/oauth_token.json */
  const char *config_path;   /* cllm config.json；預設 env LLM_CLI_CONFIG 或 ~/.config/llm/config.json */
  int open_browser;          /* 非 0＝login 時自動開瀏覽器 */
  char *err;                 /* 出錯時寫人話訊息（可 NULL）*/
  size_t err_len;
} llm_login_opts_t;

/* 跑完整登入流程（開瀏覽器→接 callback→換 token→patch cllm config）。阻塞至完成。 */
llm_login_status_t llm_login_login(const llm_login_opts_t *opts);

/* 用 refresh_token 換新 access_token 並 patch config。無 refresh → NEED_LOGIN。 */
llm_login_status_t llm_login_refresh(const llm_login_opts_t *opts);

/* 取當前有效 token（快過期自動 refresh＋patch）寫進 buf。
 * 回：>=0＝token 長度；-1＝沒登入（NEED_LOGIN）；-2＝buf 太小；-3＝其他錯（訊息在 opts->err）。*/
long llm_login_token(const llm_login_opts_t *opts, char *buf, size_t buf_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LLM_LOGIN_ABI_H */
