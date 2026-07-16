#ifndef GALTXT_CABI_CONTEXT_H
#define GALTXT_CABI_CONTEXT_H
/* cabi_context.h — 非同步控制（可選）：跨 thread 的取消 + 階段觀測。C ABI 的一塊（總覽見 cabi.h）。
 *
 * llm_ask 本身「阻塞」：在呼叫端這條 thread 跑完整條交換、handler 也在這條 thread 上被呼叫。
 * 想要非同步，就由呼叫端自己開 thread 跑 llm_ask；另一條 thread 拿同一個 ctx 取消/觀測。
 * llm_context_t 是 trivial 的（兩個 int）：呼叫端自行配置（堆疊/靜態/內嵌皆可）、{0} 或 memset
 * 歸零即用，library 不配置任何東西 → 沒有要你 free 的東西。別直接碰欄位——一律透過
 * llm_cancel()/llm_phase() 存取（正確的原子存取封在那兩個函數裡）。*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct llm_context_t {
  int cancel; /* 0＝進行中；非 0＝已請求取消 */
  int phase;  /* 目前階段，值域＝llm_phase_t（由跑 llm_ask 那條 thread 更新）*/
} llm_context_t;

/* 單一可觀測生命週期：傳輸階段 + 終局狀態。watcher thread 只讀這一個就懂全貌。*/
typedef enum {
  LLM_PHASE_IDLE,      /* 尚未開始 */
  LLM_PHASE_CONNECT,   /* 連線中（本版用 http 這層時未細分，保留給未來自持 curl）*/
  LLM_PHASE_UPLOAD,    /* 上傳請求中（同上，未細分）*/
  LLM_PHASE_WAIT,      /* 送完、等模型（server 處理中）*/
  LLM_PHASE_STREAM,    /* 接收回應中 */
  LLM_PHASE_DONE,      /* 正常完成 */
  LLM_PHASE_ERROR,     /* 失敗 */
  LLM_PHASE_CANCELLED  /* 被取消 */
} llm_phase_t;

/* 任一 thread 可呼叫：請求取消（下個安全點會乾淨收掉連線）。*/
void llm_cancel(llm_context_t *ctx);
/* 任一 thread 可呼叫：原子讀當前階段。*/
llm_phase_t llm_phase(const llm_context_t *ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GALTXT_CABI_CONTEXT_H */
