/* llm.c — cllm 的 Janet binding：把 libcllm 的 C ABI（唯一入口 llm_ask）包成一個
 * Janet 原生（native C）模組。之後 (import llm) 即得 (llm/ask …)。
 *
 * 為什麼是 native C 模組、而不是 Janet 純 FFI？Janet 的 ffi/trampoline 把回呼簽名寫死成
 * void trampoline(void* ctx, void* userdata)，表達不了 cllm 需要的
 * int on_text(const char*, size_t, void*) 這種三參數、有 int 回傳的回呼——所以走 C 橋接
 * （跟 lua/s7 同型態），在 C 端實作真正的 on_text/on_tool/on_media/on_error，內部用 Janet C API
 * （janet_pcall）回呼使用者傳進來的 JanetFunction。
 *
 * API 對齊其它語言 binding（ask + on-delta 串流回呼），Janet 慣例用 :keyword（連字號）：
 *   (import llm)
 *   (llm/ask "你好")                                   # 只給 prompt（走內建 localhost）
 *   (llm/ask "你好" "http://…/chat/completions")       # prompt + endpoint（位置形式）
 *   (llm/ask "你好" :model "local-model" :temperature 0.7)
 *   (llm/ask "數到五" :stream true                      # 串流：逐段進 :on-delta
 *            :on-delta (fn [piece] (prin piece) false))  # 回真值可中止
 * ask 一律回「完整組合後的答案字串」；失敗時：給了 :on-error 就呼叫它並回 nil，否則 janet error。
 *
 * 進階欄位（皆 :keyword，可任意組合）：
 *   :tools     (陣列 of {:name … :description … :parameters json})；:on-tool (fn [call] …)
 *              call = {:id … :name … :arguments json-str}；回真值＝中止
 *   :media     (陣列 of {:url … :mime … :bytes …})；:modalities (陣列 of {:name … :config json})
 *   :on-media  (fn [m] …)，m = {:mime … :bytes 二進位字串}；回真值＝中止
 *   :schema    JSON Schema 字串（結構化輸出，回應是 JSON 字串自己解，用 spork/json）
 *
 * 連線/取樣欄位：:endpoint :api-key :model :timeout-ms :temperature :top-p :presence-penalty
 *   :frequency-penalty :max-tokens :seed :stream。
 *
 * 消費穩定 C ABI（見 ../../docs/c-abi-reference.md）；只連 -lcllm，不需 cllm 的建置環境。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <janet.h>

#include "cabi.h"

/* ── 可增長字串緩衝：累積 on_text 逐段文字，最後一次交回 Janet（純 C 記憶體，不受 GC 影響）── */
typedef struct {
  char *p;
  size_t len, cap;
} buf;
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

/* handler 共用狀態（透過 void* user 傳給 C ABI 回呼）*/
typedef struct {
  buf acc;
  JanetFunction *on_delta, *on_error, *on_tool, *on_media; /* NULL＝未提供 */
  int cb_error;      /* Janet 回呼自己出錯 → 中止並上拋 */
  char errmsg[512];
} jctx;

/* 呼叫一個 JanetFunction（單一參數 arg），回 abort（1）／continue（0）。
 * 全程用 gcroot 護住 arg 跨越 janet_pcall；回呼內出錯 → 記訊息、設 cb_error、要求中止。*/
static int call_cb(jctx *c, JanetFunction *fn, Janet arg) {
  janet_gcroot(arg);
  Janet out = janet_wrap_nil();
  JanetSignal sig = janet_pcall(fn, 1, &arg, &out, NULL);
  janet_gcunroot(arg);
  if (sig != JANET_SIGNAL_OK) {
    JanetString s = janet_to_string(out);
    snprintf(c->errmsg, sizeof c->errmsg, "回呼出錯：%s", (const char *)s);
    c->cb_error = 1;
    return 1; /* 中止 */
  }
  return janet_truthy(out) ? 1 : 0; /* 回呼回真值＝中止 */
}

static int on_text_cb(const char *t, size_t n, void *u) {
  jctx *c = (jctx *)u;
  buf_add(&c->acc, t, n);
  if (!c->on_delta) return 0;
  int lock = janet_gclock();
  Janet arg = janet_stringv((const uint8_t *)t, (int32_t)n);
  janet_gcunlock(lock);
  return call_cb(c, c->on_delta, arg);
}

static void on_error_cb(const char *m, size_t n, void *u) {
  jctx *c = (jctx *)u;
  snprintf(c->errmsg, sizeof c->errmsg, "%.*s", (int)n, m);
  if (!c->on_error) return;
  int lock = janet_gclock();
  Janet arg = janet_stringv((const uint8_t *)m, (int32_t)n);
  janet_gcunlock(lock);
  janet_gcroot(arg);
  Janet out = janet_wrap_nil();
  (void)janet_pcall(c->on_error, 1, &arg, &out, NULL); /* on_error 內再出錯就忽略 */
  janet_gcunroot(arg);
}

/* 工具呼叫：模型每要求一個工具呼一次，交回 {:id … :name … :arguments json-str}
 * （arguments 是 JSON 字串，解析交呼叫端，如用 spork/json）。回真值＝中止。*/
static int on_tool_cb(const llm_tool_call_t *call, void *u) {
  jctx *c = (jctx *)u;
  if (!c->on_tool) return 0;
  int lock = janet_gclock();
  JanetTable *t = janet_table(3);
  janet_table_put(t, janet_ckeywordv("id"), janet_cstringv(call->id ? call->id : ""));
  janet_table_put(t, janet_ckeywordv("name"), janet_cstringv(call->name ? call->name : ""));
  janet_table_put(t, janet_ckeywordv("arguments"), janet_cstringv(call->arguments ? call->arguments : ""));
  Janet arg = janet_wrap_table(t);
  janet_gcunlock(lock);
  return call_cb(c, c->on_tool, arg);
}

/* 產出媒體：模型每產出一則媒體（如 audio）呼一次，交回 {:mime … :bytes 二進位字串}
 * （Janet string 天生二進位安全，長度看 (length …)）。回真值＝中止。*/
static int on_media_cb(const llm_media_out_t *media, void *u) {
  jctx *c = (jctx *)u;
  if (!c->on_media) return 0;
  int lock = janet_gclock();
  JanetTable *t = janet_table(2);
  janet_table_put(t, janet_ckeywordv("mime"), janet_cstringv(media->mime ? media->mime : ""));
  janet_table_put(t, janet_ckeywordv("bytes"),
                  janet_stringv((const uint8_t *)(media->data ? media->data : ""), (int32_t)media->len));
  Janet arg = janet_wrap_table(t);
  janet_gcunlock(lock);
  return call_cb(c, c->on_media, arg);
}

/* ── opts（一個 struct/table 值）取欄位小工具。字串直接借 Janet GC 堆上的指標——同步 llm_ask
 *    期間 opts 被 gcroot 護住，故指標穩定，毋須 strdup。── */
static const char *ds_str(Janet ds, const char *key) {
  Janet v = janet_get(ds, janet_ckeywordv(key));
  if (janet_checktype(v, JANET_STRING)) return (const char *)janet_unwrap_string(v);
  return NULL;
}
static JanetFunction *ds_fn(Janet ds, const char *key) {
  Janet v = janet_get(ds, janet_ckeywordv(key));
  if (janet_checktype(v, JANET_FUNCTION)) return janet_unwrap_function(v);
  return NULL;
}
static int ds_num(Janet ds, const char *key, double *out) {
  Janet v = janet_get(ds, janet_ckeywordv(key));
  if (janet_checktype(v, JANET_NUMBER)) { *out = janet_unwrap_number(v); return 1; }
  return 0;
}
static int ds_bool(Janet ds, const char *key) {
  return janet_truthy(janet_get(ds, janet_ckeywordv(key)));
}
/* 二進位欄位（如 media 的 :bytes）：借 Janet string/buffer 的位元組＋長度。*/
static const char *ds_bytes(Janet ds, const char *key, size_t *len) {
  Janet v = janet_get(ds, janet_ckeywordv(key));
  const uint8_t *d;
  int32_t n;
  if (janet_bytes_view(v, &d, &n)) { *len = (size_t)n; return (const char *)d; }
  *len = 0;
  return NULL;
}

/* (llm/ask prompt & opts) — opts 為 :keyword val … 對；第二個位置參數若為字串則當 endpoint。
 *   → 完整答案字串（成功／取消回部分）｜ nil（LLM_ERROR 且有 :on-error）｜ janet error（其它失敗）*/
static Janet cfun_ask(int32_t argc, Janet *argv) {
  janet_arity(argc, 1, -1);
  const char *prompt = (const char *)janet_getstring(argv, 0);

  int32_t kv = 1;
  const char *pos_endpoint = NULL;
  if (argc >= 2 && janet_checktype(argv[1], JANET_STRING)) {
    pos_endpoint = (const char *)janet_unwrap_string(argv[1]);
    kv = 2;
  }

  /* kv 對收成一個 table，統一用 janet_get 取欄位（含巢狀 struct 欄位）*/
  JanetTable *optsT = janet_table(argc);
  for (int32_t i = kv; i + 1 < argc; i += 2) janet_table_put(optsT, argv[i], argv[i + 1]);
  Janet opts = janet_wrap_table(optsT);
  janet_gcroot(opts); /* 護住 opts 及其借出的所有字串，跨越整個 llm_ask */

  llm_client_t c;
  memset(&c, 0, sizeof c);
  llm_request_t r;
  memset(&r, 0, sizeof r);
  r.prompt = prompt;
  llm_schema_t schema;

  const char *endpoint = ds_str(opts, "endpoint");
  c.endpoint = endpoint ? endpoint : pos_endpoint; /* NULL＝內建 localhost */
  c.api_key = ds_str(opts, "api-key");
  c.model = ds_str(opts, "model");
  double d;
  if (ds_num(opts, "timeout-ms", &d)) c.timeout_ms = (long)d;
  if (ds_num(opts, "temperature", &d)) { c.temperature = (float)d; c.field_mask |= LLM_FIELD_TEMPERATURE; }
  if (ds_num(opts, "top-p", &d)) { c.top_p = (float)d; c.field_mask |= LLM_FIELD_TOP_P; }
  if (ds_num(opts, "presence-penalty", &d)) { c.presence_penalty = (float)d; c.field_mask |= LLM_FIELD_PRESENCE_PENALTY; }
  if (ds_num(opts, "frequency-penalty", &d)) { c.frequency_penalty = (float)d; c.field_mask |= LLM_FIELD_FREQUENCY_PENALTY; }
  if (ds_num(opts, "max-tokens", &d)) { c.max_tokens = (int)d; c.field_mask |= LLM_FIELD_MAX_TOKENS; }
  if (ds_num(opts, "seed", &d)) { c.seed = (int)d; c.field_mask |= LLM_FIELD_SEED; }
  r.stream = ds_bool(opts, "stream") ? 1 : 0;
  const char *schema_json = ds_str(opts, "schema");
  if (schema_json) { schema.name = "response"; schema.json = schema_json; r.schema = &schema; }

  /* :tools / :media / :modalities：陣列（tuple/array）→ 只需 malloc 結構陣列本身，
   * 內部字串借 opts（已 gcroot）的 Janet 字串。call 完 free 陣列即可。*/
  llm_tool_def_t *tools = NULL;
  size_t tools_n = 0;
  llm_media_in_t *media = NULL;
  size_t media_n = 0;
  llm_modality_t *mods = NULL;
  size_t mods_n = 0;
  const Janet *items;
  int32_t n;

  if (janet_indexed_view(janet_get(opts, janet_ckeywordv("tools")), &items, &n) && n > 0) {
    tools = (llm_tool_def_t *)calloc((size_t)n, sizeof *tools);
    for (int32_t i = 0; i < n; i++) {
      tools[i].name = ds_str(items[i], "name");
      tools[i].description = ds_str(items[i], "description");
      tools[i].parameters = ds_str(items[i], "parameters");
    }
    tools_n = (size_t)n;
  }
  if (janet_indexed_view(janet_get(opts, janet_ckeywordv("media")), &items, &n) && n > 0) {
    media = (llm_media_in_t *)calloc((size_t)n, sizeof *media);
    for (int32_t i = 0; i < n; i++) {
      media[i].url = ds_str(items[i], "url");
      media[i].mime = ds_str(items[i], "mime");
      media[i].data = ds_bytes(items[i], "bytes", &media[i].len);
    }
    media_n = (size_t)n;
  }
  if (janet_indexed_view(janet_get(opts, janet_ckeywordv("modalities")), &items, &n) && n > 0) {
    mods = (llm_modality_t *)calloc((size_t)n, sizeof *mods);
    for (int32_t i = 0; i < n; i++) {
      mods[i].name = ds_str(items[i], "name");
      mods[i].config = ds_str(items[i], "config");
    }
    mods_n = (size_t)n;
  }
  r.tools = tools;
  r.tools_count = tools_n;
  r.media = media;
  r.media_count = media_n;
  r.modalities = mods;
  r.modalities_count = mods_n;

  jctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.on_delta = ds_fn(opts, "on-delta");
  ctx.on_error = ds_fn(opts, "on-error");
  ctx.on_tool = ds_fn(opts, "on-tool");
  ctx.on_media = ds_fn(opts, "on-media");

  llm_handlers_t h;
  memset(&h, 0, sizeof h);
  h.on_text = on_text_cb;
  h.text_user = &ctx;
  h.on_error = on_error_cb;
  h.error_user = &ctx;
  h.on_tool = on_tool_cb;
  h.tool_user = &ctx;
  h.on_media = on_media_cb;
  h.media_user = &ctx;

  llm_status_t st = llm_ask(NULL, &c, &r, &h);

  janet_gcunroot(opts); /* 之後只需 ctx.acc（C 記憶體），opts 可釋 */
  free(tools);
  free(media);
  free(mods);

  if (ctx.cb_error) { /* Janet 回呼丟的錯：清乾淨後上拋成 janet error */
    char msg[512];
    snprintf(msg, sizeof msg, "%s", ctx.errmsg);
    free(ctx.acc.p);
    janet_panicf("%s", msg);
  }
  if (st == LLM_OK || st == LLM_CANCELLED) { /* 取消＝回已收到的部分 */
    Janet result = janet_stringv((const uint8_t *)(ctx.acc.p ? ctx.acc.p : ""), (int32_t)ctx.acc.len);
    free(ctx.acc.p);
    return result;
  }
  /* LLM_ERROR */
  if (ctx.on_error) { /* on_error 已被呼叫 → 回 nil */
    free(ctx.acc.p);
    return janet_wrap_nil();
  }
  char msg[512];
  snprintf(msg, sizeof msg, "%s", ctx.errmsg[0] ? ctx.errmsg : "llm_ask failed");
  free(ctx.acc.p);
  janet_panicf("%s", msg);
}

static const JanetReg cfuns[] = {
    {"ask", cfun_ask,
     "(llm/ask prompt & opts)\n\n"
     "問 LLM，回完整答案字串。opts 為 :keyword val … 對（連字號慣例）：\n"
     ":endpoint :api-key :model :timeout-ms :temperature :top-p :presence-penalty\n"
     ":frequency-penalty :max-tokens :seed :stream :schema；回呼 :on-delta :on-error\n"
     ":on-tool :on-media（回真值＝中止）；進階 :tools :media :modalities。\n"
     "第二個位置參數若為字串則當 endpoint。失敗時給了 :on-error 回 nil，否則 error。"},
    {NULL, NULL, NULL},
};

/* (import llm) 進入點 */
JANET_MODULE_ENTRY(JanetTable *env) {
  janet_cfuns(env, "llm", cfuns);
}
