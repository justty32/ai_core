#ifndef GALTXT_CABI_RESPONSE_H
#define GALTXT_CABI_RESPONSE_H
/* cabi_response.h — 一次發問的「收回」：模型產出的 tool_call／media_out ＋ 出口回呼 handlers。
 * C ABI 的一塊（總覽見 cabi.h）。「送出」那半邊（schema／tool_def／media_in…）見 cabi_request.h。 */

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* 工具「呼叫」（模型回來要你執行的）。*/
typedef struct llm_tool_call_t {
  const char *id;
  const char *name;
  const char *arguments; /* 模型產生的 arguments（JSON 字串）*/
} llm_tool_call_t;

/* (b) 產出媒體＝回應的輸出內容（模型生成的 audio…）。純位元組，無 url，種類看 mime。*/
typedef struct llm_media_out_t {
  const char *mime;
  const char *data; /* 原始位元組 */
  size_t len;
} llm_media_out_t;

/* token 用量（後端回報的 usage）。-1＝該欄後端沒給。*/
typedef struct llm_usage_t {
  int prompt_tokens;
  int completion_tokens;
  int total_tokens;
} llm_usage_t;

/* ── 出口回呼（都帶 void* user 以攜狀態）── */
/* 文字：串流逐段／非串流整段一次。text 非 NUL 結尾保證，長度看 len。回非 0＝要求中止串流。*/
typedef int (*llm_text_handler)(const char *text, size_t len, void *user);
/* 工具呼叫：模型每要求一個工具呼一次（永遠拼完整才交付）。回非 0＝要求中止。*/
typedef int (*llm_tool_handler)(const llm_tool_call_t *call, void *user);
/* 媒體輸出：模型每產出一則媒體（如 audio）呼一次；media->data/len＝原始位元組。回非 0＝中止。*/
typedef int (*llm_media_handler)(const llm_media_out_t *media, void *user);
/* 錯誤：傳輸失敗／後端回錯時呼叫一次。message 只在回呼期間有效。*/
typedef void (*llm_error_handler)(const char *message, size_t len, void *user);
/* 用量：後端有回 usage 時、內容 handler 都跑完後呼叫至多一次。usage 只在回呼期間有效。
 * ⚠ 串流時裝上本 handler＝請求會多送 stream_options.include_usage（要後端把 usage 附在末塊）；
 *   不裝就完全不送、行為與從前一致。*/
typedef void (*llm_usage_handler)(const llm_usage_t *usage, void *user);

/* 出口回呼集（任一 handler 可為 NULL）。on_usage 排最後＝尾端追加，舊客戶端歸零重編即相容。*/
typedef struct llm_handlers_t {
  llm_text_handler on_text;
  void *text_user;
  llm_tool_handler on_tool;
  void *tool_user;
  llm_media_handler on_media; /* 模型產出的媒體（如 audio）*/
  void *media_user;
  llm_error_handler on_error;
  void *error_user;
  llm_usage_handler on_usage; /* token 用量（NULL＝不要，串流時也不多送 stream_options）*/
  void *usage_user;
} llm_handlers_t;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GALTXT_CABI_RESPONSE_H */
