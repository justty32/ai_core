#ifndef GALTXT_CABI_REQUEST_H
#define GALTXT_CABI_REQUEST_H
/* cabi_request.h — 一次發問的「輸入」：schema／tool 定義／輸入媒體／想要的輸出模態／request。
 * C ABI 的一塊（總覽見 cabi.h）。「收回」那半邊（tool_call／media_out／handlers）見 cabi_response.h。 */

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* 結構化輸出的 schema。*/
typedef struct llm_schema_t {
  const char *name; /* NULL＝"response" */
  const char *json; /* JSON Schema「物件」字串 */
} llm_schema_t;

/* 工具「定義」（你送出去給模型看的）。*/
typedef struct llm_tool_def_t {
  const char *name;
  const char *description;
  const char *parameters; /* 參數的 JSON Schema（物件字串）*/
} llm_tool_def_t;

/* 三個媒體角色三個型（別讓「輸入媒體／想要的輸出／產出媒體」共用一個名）；種類一律從 mime 看。
 * 產出媒體 llm_media_out_t 屬「收回」，放在 cabi_response.h。*/

/* (a) 輸入媒體＝請求的輸入內容（圖／音給模型看）。url 或 data 二選一（都給時 url 優先）：
 *     · url ＝"https://…"（後端自己抓）或 "data:<mime>;base64,…"（已內嵌的 data URI）。
 *     · data/len ＝直接給原始位元組（本地檔讀進來就傳，免自己 base64；impl 內部轉 data URI）。*/
typedef struct llm_media_in_t {
  const char *url;  /* http url 或 data URI */
  const char *mime; /* 種類（走 data URI 時已內含可 NULL；走 data 時必帶）*/
  const char *data; /* 或直接給原始位元組（免先 base64）*/
  size_t len;       /* data 的位元組長度（走 url 時為 0）*/
} llm_media_in_t;

/* (c) 想要的輸出模態＝請求的「指令」（不是媒體！）——要模型產哪些模態、各帶什麼生成參數。
 *     name＝"text"/"audio"/"image"/"video"，config＝該模態生成參數的 JSON 物件（NULL/""＝默認）。
 *     例：audio → {"voice":"alloy","format":"wav"}；image → {"size":"1024x1024"}。*/
typedef struct llm_modality_t {
  const char *name;
  const char *config; /* 該模態生成參數（JSON 物件字串）*/
} llm_modality_t;

/* 一次發問的輸入（schema／tools／media／modalities 可任意組合；stream 與 tools 正交——
 * text/schema/media 皆可串流，tool_calls 一律拼完整才交給 on_tool，不受 stream 影響）。*/
typedef struct llm_request_t {
  const char *prompt;          /* 必填 */
  const llm_schema_t *schema;  /* NULL＝不用結構化輸出 */
  const llm_tool_def_t *tools;
  size_t tools_count;
  const llm_media_in_t *media; /* (a) 輸入媒體（圖／音…）*/
  size_t media_count;
  const llm_modality_t *modalities; /* (c) 想要的輸出模態＋參數；NULL/0＝只文字 */
  size_t modalities_count;
  int stream; /* 非 0＝串流（text/schema/media 逐段；tool_calls 仍一次性交付）*/
} llm_request_t;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GALTXT_CABI_REQUEST_H */
