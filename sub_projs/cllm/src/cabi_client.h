#ifndef GALTXT_CABI_CLIENT_H
#define GALTXT_CABI_CLIENT_H
/* cabi_client.h — 呼叫端設定：連線 + 取樣。C ABI 的一塊（總覽見 cabi.h）。 */

#ifdef __cplusplus
extern "C" {
#endif

/* 取樣屬性「哪些有設」的位旗標（bit 值，可 | 起來）。沿用 statx stx_mask 慣例：
 * 欄位名 field_mask（名詞、明確），旗標 LLM_FIELD_*。指標欄（endpoint/api_key/model）
 * 用 NULL＝未設，不佔 bit；遮罩只管 6 個數值取樣欄位。*/
enum {
  LLM_FIELD_TEMPERATURE = 1u << 0,
  LLM_FIELD_TOP_P = 1u << 1,
  LLM_FIELD_PRESENCE_PENALTY = 1u << 2,
  LLM_FIELD_FREQUENCY_PENALTY = 1u << 3,
  LLM_FIELD_MAX_TOKENS = 1u << 4,
  LLM_FIELD_SEED = 1u << 5
};

/* 呼叫端設定：連線 + 取樣。數值取樣欄位是否生效看 field_mask（用 LLM_FIELD_* 標）。*/
typedef struct llm_client_t {
  const char *endpoint; /* NULL＝內建預設 http://localhost:1234/v1/chat/completions */
  const char *api_key;  /* NULL／""＝不送 Authorization: Bearer */
  const char *model;    /* NULL＝不送 model（本地後端＝用已載入模型）*/
  long timeout_ms;      /* 0＝不設逾時 */
  unsigned field_mask;  /* 哪些數值取樣欄位有設（LLM_FIELD_* 的 OR）*/
  float temperature, top_p, presence_penalty, frequency_penalty;
  int max_tokens, seed;
} llm_client_t;

/* field_mask 的設/查（也可直接 c->field_mask = LLM_FIELD_TEMPERATURE | LLM_FIELD_SEED）。*/
static inline void llm_client_set_field(llm_client_t *c, unsigned field, int on) {
  if (on)
    c->field_mask |= field;
  else
    c->field_mask &= ~field;
}
static inline int llm_client_has_field(const llm_client_t *c, unsigned field) {
  return (c->field_mask & field) != 0;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GALTXT_CABI_CLIENT_H */
