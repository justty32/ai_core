/* bridge.c — bridge.h 的定義：把 Go export 的 handler 掛進 llm_handlers_t。 */
#include "bridge.h"
#include "_cgo_export.h" /* cgo 生成：cllmGoOnText/cllmGoOnError/cllmGoOnTool/cllmGoOnMedia 的正確宣告 */

void cllm_fill_handlers(llm_handlers_t *h, uintptr_t user) {
  /* cgo 生成的 export 參數是 char*／非 const 的 struct 指標，C ABI typedef 是 const 版本——
   * ABI 相同、僅型別修飾差異，cast 到目標函數指標型別（const-ness 不影響呼叫慣例，安全）。*/
  h->on_text = (llm_text_handler)cllmGoOnText;
  h->text_user = (void *)user; /* 不透明往返：uintptr（cgo.Handle）↔ void* */
  h->on_error = (llm_error_handler)cllmGoOnError;
  h->error_user = (void *)user;
  h->on_tool = (llm_tool_handler)cllmGoOnTool;
  h->tool_user = (void *)user;
  h->on_media = (llm_media_handler)cllmGoOnMedia;
  h->media_user = (void *)user;
}
