/* llm_s7.c — cllm 的 s7 (Scheme) binding：把 libcllm 的 C ABI（唯一入口 llm_ask）
 * 註冊成一個 s7 foreign function `llm-ask`。
 *
 * API 對齊 galtxt/try_4 既有慣例（llm-ask + :on-delta 串流回呼、keyword 選項）：
 *   (llm-ask "你好")                                    ; 只給 prompt（走內建 localhost）
 *   (llm-ask "你好" "http://…/chat/completions")        ; prompt + endpoint（位置形式）
 *   (llm-ask "你好" :endpoint … :model … :temperature 0.7)          ; keyword 選項
 *   (llm-ask "數到五" :stream #t                         ; 串流：逐段進 on-delta
 *            :on-delta (lambda (piece) (display piece) #f))          ; 回 #t 可中止
 * 一律回「完整組合後的答案字串」；失敗時：有 :on-error 就呼叫它並回 #f，否則 (error 'llm-error …)。
 *
 * 消費穩定 C ABI（見 ../../docs/c-abi-reference.md）；只連 -lcllm。
 * 範圍（v1，刻意小）：prompt＋連線/取樣選項＋stream＋schema＋on-delta/on-error。
 *   tools／media／modalities 未收。
 *
 * ⚠ 回呼別在裡面丟 Scheme error：on-delta/on-error 會在 llm_ask（C++）執行途中被呼叫，
 *   s7 error 以 longjmp 實作，穿過 C++ 堆疊是未定義行為。回呼請只做無害的事（display 等）。
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "s7.h"
#include "cabi.h"

typedef struct { char *p; size_t len, cap; } buf;
static void buf_add(buf *b, const char *s, size_t n) {
  if (b->len + n + 1 > b->cap) {
    size_t c = b->cap ? b->cap : 256;
    while (c < b->len + n + 1) c *= 2;
    b->p = (char *)realloc(b->p, c);
    b->cap = c;
  }
  memcpy(b->p + b->len, s, n);
  b->len += n;
  b->p[b->len] = 0;
}

typedef struct {
  s7_scheme *sc;
  buf acc;
  s7_pointer on_delta, on_error; /* NULL＝未給 */
  char errmsg[512];
} sctx;

static int on_text_cb(const char *t, size_t n, void *u) {
  sctx *c = (sctx *)u;
  buf_add(&c->acc, t, n);
  if (c->on_delta) {
    s7_pointer res = s7_call(c->sc, c->on_delta,
                             s7_list(c->sc, 1, s7_make_string_with_length(c->sc, t, (s7_int)n)));
    if (res == s7_t(c->sc)) return 1; /* 回 #t 中止串流 */
  }
  return 0;
}
static void on_error_cb(const char *m, size_t n, void *u) {
  sctx *c = (sctx *)u;
  snprintf(c->errmsg, sizeof c->errmsg, "%.*s", (int)n, m);
  if (c->on_error)
    s7_call(c->sc, c->on_error, s7_list(c->sc, 1, s7_make_string_with_length(c->sc, m, (s7_int)n)));
}

/* 在 keyword 選項串 rest（:k v :k v …）裡找 name 對應的值；沒有回 NULL。*/
static s7_pointer kw_get(s7_scheme *sc, s7_pointer rest, const char *name) {
  s7_pointer kw = s7_make_keyword(sc, name); /* 內建 interned，可用指標相等比對 */
  for (s7_pointer p = rest; s7_is_pair(p) && s7_is_pair(s7_cdr(p)); p = s7_cddr(p))
    if (s7_car(p) == kw) return s7_cadr(p);
  return NULL;
}

/* (llm-ask prompt [endpoint] | prompt :k v …) → answer string */
static s7_pointer g_llm_ask(s7_scheme *sc, s7_pointer args) {
  s7_pointer po = s7_car(args);
  if (!s7_is_string(po))
    return s7_wrong_type_arg_error(sc, "llm-ask", 1, po, "a string (prompt)");

  llm_client_t c;  memset(&c, 0, sizeof c);
  llm_request_t r; memset(&r, 0, sizeof r);
  r.prompt = s7_string(po); /* 指向 args 內的 s7 字串，llm_ask 期間有效 */
  llm_schema_t schema;
  sctx ctx; memset(&ctx, 0, sizeof ctx); ctx.sc = sc;

  s7_pointer rest = s7_cdr(args);
  if (s7_is_pair(rest)) {
    s7_pointer first = s7_car(rest);
    if (s7_is_keyword(first)) { /* keyword 選項形式 */
      s7_pointer v;
      if ((v = kw_get(sc, rest, "endpoint"))    && s7_is_string(v))  c.endpoint = s7_string(v);
      if ((v = kw_get(sc, rest, "api-key"))     && s7_is_string(v))  c.api_key  = s7_string(v);
      if ((v = kw_get(sc, rest, "model"))       && s7_is_string(v))  c.model    = s7_string(v);
      if ((v = kw_get(sc, rest, "timeout-ms"))  && s7_is_integer(v)) c.timeout_ms = (long)s7_integer(v);
      if ((v = kw_get(sc, rest, "temperature")) && s7_is_number(v))  { c.temperature       = (float)s7_number_to_real(sc, v); c.field_mask |= LLM_FIELD_TEMPERATURE; }
      if ((v = kw_get(sc, rest, "top-p"))       && s7_is_number(v))  { c.top_p             = (float)s7_number_to_real(sc, v); c.field_mask |= LLM_FIELD_TOP_P; }
      if ((v = kw_get(sc, rest, "presence-penalty"))  && s7_is_number(v)) { c.presence_penalty  = (float)s7_number_to_real(sc, v); c.field_mask |= LLM_FIELD_PRESENCE_PENALTY; }
      if ((v = kw_get(sc, rest, "frequency-penalty")) && s7_is_number(v)) { c.frequency_penalty = (float)s7_number_to_real(sc, v); c.field_mask |= LLM_FIELD_FREQUENCY_PENALTY; }
      if ((v = kw_get(sc, rest, "max-tokens"))  && s7_is_integer(v)) { c.max_tokens = (int)s7_integer(v); c.field_mask |= LLM_FIELD_MAX_TOKENS; }
      if ((v = kw_get(sc, rest, "seed"))        && s7_is_integer(v)) { c.seed       = (int)s7_integer(v); c.field_mask |= LLM_FIELD_SEED; }
      if ((v = kw_get(sc, rest, "stream")))     r.stream = (v != s7_f(sc)) ? 1 : 0;
      if ((v = kw_get(sc, rest, "schema"))      && s7_is_string(v))    { schema.name = "response"; schema.json = s7_string(v); r.schema = &schema; }
      if ((v = kw_get(sc, rest, "on-delta"))    && s7_is_procedure(v)) ctx.on_delta = v;
      if ((v = kw_get(sc, rest, "on-error"))    && s7_is_procedure(v)) ctx.on_error = v;
    } else if (s7_is_string(first)) { /* 位置形式：第二參數＝endpoint */
      c.endpoint = s7_string(first);
    }
  }

  llm_handlers_t h; memset(&h, 0, sizeof h);
  h.on_text  = on_text_cb;  h.text_user  = &ctx;
  h.on_error = on_error_cb; h.error_user = &ctx;

  llm_status_t st = llm_ask(NULL, &c, &r, &h);

  s7_pointer result;
  if (st == LLM_OK) {
    result = s7_make_string_with_length(sc, ctx.acc.p ? ctx.acc.p : "", (s7_int)ctx.acc.len);
  } else if (ctx.on_error) {
    result = s7_f(sc); /* on-error 已被呼叫 */
  } else {
    char msg[512]; snprintf(msg, sizeof msg, "%s", ctx.errmsg[0] ? ctx.errmsg : "llm_ask failed");
    free(ctx.acc.p);
    return s7_error(sc, s7_make_symbol(sc, "llm-error"), s7_list(sc, 1, s7_make_string(sc, msg)));
  }
  free(ctx.acc.p);
  return result;
}

/* 註冊到既有的 s7 直譯器（自帶 host 或嵌入端皆可呼叫）。*/
void llm_s7_init(s7_scheme *sc) {
  s7_define_function(
      sc, "llm-ask", g_llm_ask, 1, 0, true,
      "(llm-ask prompt [endpoint] | prompt :endpoint s :model s :temperature x :stream #t "
      ":on-delta proc :on-error proc :schema json …) 透過 libcllm 問 LLM，回完整答案字串");
}
