#ifndef GALTXT_CABI_H
#define GALTXT_CABI_H
/* cabi.h — galtxt try_3 對外 C ABI 的「總覽傘檔」。純 C 可 include。
 *
 * 按功能拆成數個子檔，這裡全數 include——客戶端只要 #include "cabi.h" 就拿到整個 ABI：
 *   · cabi_client.h    連線 + 取樣設定（llm_client_t、LLM_FIELD_*）
 *   · cabi_request.h   一次發問的「輸入」（schema／tool_def／media_in／modality／request）
 *   · cabi_response.h  一次發問的「收回」（tool_call／media_out／handlers）
 *   · cabi_context.h   非同步控制（context／phase／cancel）
 * 本傘檔自身再補上跨這些型的入口：llm_status_t ＋ llm_ask。
 *
 * 落地說明：各子檔已是標準 C 的 `extern "C"` 守衛（C 沒有 namespace，符號不能被 C++ mangle 才能
 * 跨語言連結），size_t 走 <stddef.h>。設計取捨與逐條判斷見同資料夾的 advice.hpp（設計提案原稿）。
 * 唯一入口 llm_ask 一次吃 prompt＋schema＋tools＋media＋modalities＋stream，出口走 handlers；
 * 實作在 cabi_*.cpp（C++，用 glaze 組請求、接 http 傳輸管子）。
 *
 * 記憶體約定：本 ABI 不配置任何要呼叫端釋放的東西——沒有 llm_free、沒有配置器。
 *   本地媒體檔讀進 buffer，用 llm_media_in_t 的 data/len＋mime 直接傳（impl 內部 base64）。
 */

#include "cabi_client.h"
#include "cabi_context.h"
#include "cabi_request.h"
#include "cabi_response.h"

#ifdef __cplusplus
extern "C" {
#endif

/* llm_ask 的回傳：把「被取消」跟「出錯」分開（>0＝錯、<0＝取消、0＝成功）。*/
typedef enum {
  LLM_CANCELLED = -1, /* 被取消（phase 也會落在 LLM_PHASE_CANCELLED）*/
  LLM_OK = 0,         /* 成功 */
  LLM_ERROR = 1       /* 傳輸/後端錯誤（on_error 已被呼叫）*/
} llm_status_t;

/* ★ 統一發問（阻塞）。ctx 可為 NULL（不需要取消/觀測時，退回零負擔呼叫）。
 *   回 LLM_OK＝成功；LLM_ERROR＝失敗（若有 on_error 會被呼叫一次帶訊息）；
 *   LLM_CANCELLED＝被取消。所有輸出走 handlers。*/
llm_status_t llm_ask(llm_context_t *ctx, const llm_client_t *client,
                     const llm_request_t *req, const llm_handlers_t *handlers);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GALTXT_CABI_H */
