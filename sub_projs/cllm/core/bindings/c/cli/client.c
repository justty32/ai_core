/* client.c — 依欄位名把值寫進 llm_client_t（config／旗標共用；對齊 llm_client_t 欄位語意）。 */
#include "client.h"
#include <stdlib.h>
#include <string.h>

void owned_free(owned_strings_t *o) {
  free(o->endpoint); free(o->api_key); free(o->model);
  o->endpoint = o->api_key = o->model = NULL;
}

/* 複製字串到 slot，先釋放舊值（config 後被旗標覆寫時不漏）。回指向新複本的指標。 */
static char *dup_into(char **slot, const char *val) {
  free(*slot);
  *slot = val ? strdup(val) : NULL;
  return *slot;
}

void client_set_number(llm_client_t *c, const char *field, double v) {
  if (!strcmp(field, "timeout_ms")) { c->timeout_ms = (long)v; return; }
  if (!strcmp(field, "temperature")) { c->temperature = (float)v; c->field_mask |= LLM_FIELD_TEMPERATURE; return; }
  if (!strcmp(field, "top_p")) { c->top_p = (float)v; c->field_mask |= LLM_FIELD_TOP_P; return; }
  if (!strcmp(field, "presence_penalty")) { c->presence_penalty = (float)v; c->field_mask |= LLM_FIELD_PRESENCE_PENALTY; return; }
  if (!strcmp(field, "frequency_penalty")) { c->frequency_penalty = (float)v; c->field_mask |= LLM_FIELD_FREQUENCY_PENALTY; return; }
  if (!strcmp(field, "max_tokens")) { c->max_tokens = (int)v; c->field_mask |= LLM_FIELD_MAX_TOKENS; return; }
  if (!strcmp(field, "seed")) { c->seed = (int)v; c->field_mask |= LLM_FIELD_SEED; return; }
}

int client_set_from_str(llm_client_t *c, owned_strings_t *o,
                        const char *field, flag_type_t type, const char *val) {
  if (type == F_STR) {
    if (!strcmp(field, "endpoint")) c->endpoint = dup_into(&o->endpoint, val);
    else if (!strcmp(field, "api_key")) c->api_key = dup_into(&o->api_key, val);
    else if (!strcmp(field, "model")) c->model = dup_into(&o->model, val);
    return 0;
  }
  /* 數值：strto* 並驗整段吃完，壞值回 -1（對齊 cli.cpp 的型別錯＝用法錯）。 */
  char *end = NULL;
  if (type == F_FLOAT) {
    double d = strtod(val, &end);
    if (end == val || *end) return -1;
    client_set_number(c, field, d);
  } else { /* F_INT / F_LONG */
    long l = strtol(val, &end, 10);
    if (end == val || *end) return -1;
    client_set_number(c, field, (double)l);
  }
  return 0;
}
