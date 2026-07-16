/* bridge.c — bridge.h 的定義：把 Go export 的 handler 掛進 llm_handlers_t。 */
#include "bridge.h"
#include "_cgo_export.h" /* cgo 生成：cllmGoOnText / cllmGoOnError 的正確宣告 */

void cllm_fill_handlers(llm_handlers_t *h, uintptr_t user) {
  /* cgo 生成的 export 參數是 char*，C ABI typedef 是 const char*——ABI 相同、僅型別修飾差異，
   * cast 到目標函數指標型別（const-ness 不影響呼叫慣例，安全）。*/
  h->on_text = (llm_text_handler)cllmGoOnText;
  h->text_user = (void *)user; /* 不透明往返：uintptr（cgo.Handle）↔ void* */
  h->on_error = (llm_error_handler)cllmGoOnError;
  h->error_user = (void *)user;
  /* on_tool/on_media 保持 NULL：CLI/此綁定不開 tools/modalities */
}
