/* llm.c — cllm 的 Lua binding：把 libcllm 的 C ABI（唯一入口 llm_ask）包成一個 Lua 模組。
 *
 * API 對齊 galtxt/try_4 既有慣例（llm.ask + on_delta 串流回呼）：
 *   local llm = require("llm")
 *   local ans = llm.ask("你好")                              -- 只給 prompt（走內建 localhost）
 *   local ans = llm.ask("你好", "http://…/chat/completions") -- prompt + endpoint（位置形式）
 *   local ans = llm.ask{ prompt="你好", endpoint=…, model=…, temperature=0.7 }   -- table 形式
 *   llm.ask{ prompt="數到五", stream=true,                   -- 串流：逐段進 on_delta
 *            on_delta=function(piece) io.write(piece); return false end }  -- 回 true 可中止
 * ask 一律回「完整組合後的答案字串」；失敗回 nil, errmsg。
 *
 * 消費穩定 C ABI（見 ../../docs/c-abi-reference.md）；只連 -lcllm，不需 cllm 的建置環境。
 * 範圍（v1，刻意小）：prompt＋連線/取樣選項＋stream＋schema（結構化輸出）＋on_delta/on_error。
 *   tools／media／modalities 未收（要的話照 llm_request_t 補）。
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "lua.h"
#include "lauxlib.h"

#include "cabi.h"

/* ── 可增長字串緩衝：累積 on_text 逐段文字，最後一次交回 Lua ── */
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

/* handler 共用狀態（透過 void* user 傳給 C ABI 回呼）*/
typedef struct {
  lua_State *L;
  buf acc;
  int on_delta_ref, on_error_ref; /* LUA_REGISTRYINDEX ref，或 LUA_NOREF */
  int cb_error;                   /* Lua 回呼自己丟了錯 → 中止並上拋 */
  char errmsg[512];
} lctx;

static int on_text_cb(const char *t, size_t n, void *u) {
  lctx *c = (lctx *)u;
  buf_add(&c->acc, t, n);
  if (c->on_delta_ref == LUA_NOREF) return 0;
  lua_State *L = c->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, c->on_delta_ref);
  lua_pushlstring(L, t, n);
  if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
    snprintf(c->errmsg, sizeof c->errmsg, "on_delta 回呼出錯：%s", lua_tostring(L, -1));
    lua_pop(L, 1);
    c->cb_error = 1;
    return 1; /* 中止 */
  }
  int abort = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return abort; /* 回呼回 true 即中止串流 */
}

static void on_error_cb(const char *m, size_t n, void *u) {
  lctx *c = (lctx *)u;
  snprintf(c->errmsg, sizeof c->errmsg, "%.*s", (int)n, m);
  if (c->on_error_ref == LUA_NOREF) return;
  lua_State *L = c->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, c->on_error_ref);
  lua_pushlstring(L, m, n);
  (void)lua_pcall(L, 1, 0, 0); /* on_error 內再出錯就忽略 */
}

/* ── opts table 取值小工具（欄位缺就回預設，不強制型別）── */
static char *dup_field(lua_State *L, int t, const char *k) {
  lua_getfield(L, t, k);
  char *r = lua_isstring(L, -1) ? strdup(lua_tostring(L, -1)) : NULL;
  lua_pop(L, 1);
  return r;
}
static int ref_field(lua_State *L, int t, const char *k) {
  lua_getfield(L, t, k);
  if (lua_isfunction(L, -1)) return luaL_ref(L, LUA_REGISTRYINDEX); /* 彈出並存 ref */
  lua_pop(L, 1);
  return LUA_NOREF;
}
static int set_f(lua_State *L, int t, const char *k, float *dst) {
  lua_getfield(L, t, k);
  int ok = lua_isnumber(L, -1);
  if (ok) *dst = (float)lua_tonumber(L, -1);
  lua_pop(L, 1);
  return ok;
}
static int set_i(lua_State *L, int t, const char *k, int *dst) {
  lua_getfield(L, t, k);
  int ok = lua_isnumber(L, -1);
  if (ok) *dst = (int)lua_tointeger(L, -1);
  lua_pop(L, 1);
  return ok;
}

/* llm.ask(prompt [, endpoint])｜llm.ask(prompt, opts)｜llm.ask(opts)
 *   → text（成功）｜ nil, errmsg（失敗/取消）*/
static int l_ask(lua_State *L) {
  int optidx = 0;               /* opts table 的堆疊位置，0＝無 */
  char *prompt_dup = NULL;      /* table 形式時 strdup 的 prompt */
  const char *prompt;
  char *endpoint = NULL, *api_key = NULL, *model = NULL, *schema_json = NULL;
  const char *pos_endpoint = NULL;

  if (lua_istable(L, 1)) {                      /* llm.ask{ prompt=… } */
    optidx = 1;
    lua_getfield(L, 1, "prompt");
    prompt_dup = strdup(luaL_checkstring(L, -1));
    lua_pop(L, 1);
    prompt = prompt_dup;
  } else {                                      /* llm.ask("…", …) */
    prompt = luaL_checkstring(L, 1);            /* 錨在 index 1，llm_ask 期間有效 */
    if (lua_type(L, 2) == LUA_TSTRING) pos_endpoint = lua_tostring(L, 2);
    else if (lua_istable(L, 2)) optidx = 2;
  }

  llm_client_t c;  memset(&c, 0, sizeof c);
  llm_request_t r; memset(&r, 0, sizeof r);
  r.prompt = prompt;
  llm_schema_t schema;
  lctx ctx; memset(&ctx, 0, sizeof ctx);
  ctx.L = L; ctx.on_delta_ref = LUA_NOREF; ctx.on_error_ref = LUA_NOREF;

  if (optidx) {
    endpoint = dup_field(L, optidx, "endpoint"); /* NULL = 內建 localhost */
    api_key  = dup_field(L, optidx, "api_key");
    model    = dup_field(L, optidx, "model");
    lua_getfield(L, optidx, "timeout_ms");
    if (lua_isnumber(L, -1)) c.timeout_ms = (long)lua_tointeger(L, -1);
    lua_pop(L, 1);
    if (set_f(L, optidx, "temperature",       &c.temperature))       c.field_mask |= LLM_FIELD_TEMPERATURE;
    if (set_f(L, optidx, "top_p",             &c.top_p))             c.field_mask |= LLM_FIELD_TOP_P;
    if (set_f(L, optidx, "presence_penalty",  &c.presence_penalty))  c.field_mask |= LLM_FIELD_PRESENCE_PENALTY;
    if (set_f(L, optidx, "frequency_penalty", &c.frequency_penalty)) c.field_mask |= LLM_FIELD_FREQUENCY_PENALTY;
    if (set_i(L, optidx, "max_tokens",        &c.max_tokens))        c.field_mask |= LLM_FIELD_MAX_TOKENS;
    if (set_i(L, optidx, "seed",              &c.seed))              c.field_mask |= LLM_FIELD_SEED;
    lua_getfield(L, optidx, "stream"); r.stream = lua_toboolean(L, -1); lua_pop(L, 1);
    schema_json = dup_field(L, optidx, "schema");
    ctx.on_delta_ref = ref_field(L, optidx, "on_delta");
    ctx.on_error_ref = ref_field(L, optidx, "on_error");
  }
  c.endpoint = endpoint ? endpoint : pos_endpoint;
  c.api_key = api_key;
  c.model = model;
  if (schema_json) { schema.name = "response"; schema.json = schema_json; r.schema = &schema; }

  llm_handlers_t h; memset(&h, 0, sizeof h);
  h.on_text  = on_text_cb;  h.text_user  = &ctx;
  h.on_error = on_error_cb; h.error_user = &ctx;

  llm_status_t st = llm_ask(NULL, &c, &r, &h);

  if (ctx.on_delta_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ctx.on_delta_ref);
  if (ctx.on_error_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ctx.on_error_ref);
  free(endpoint); free(api_key); free(model); free(schema_json); free(prompt_dup);

  if (ctx.cb_error) { /* Lua 回呼丟的錯：清乾淨後上拋成 Lua error */
    char msg[512]; snprintf(msg, sizeof msg, "%s", ctx.errmsg);
    free(ctx.acc.p);
    return luaL_error(L, "%s", msg);
  }
  int rc;
  if (st == LLM_OK) {
    lua_pushlstring(L, ctx.acc.p ? ctx.acc.p : "", ctx.acc.len);
    rc = 1;
  } else {
    lua_pushnil(L);
    lua_pushstring(L, st == LLM_CANCELLED ? "cancelled"
                       : (ctx.errmsg[0] ? ctx.errmsg : "llm_ask failed"));
    rc = 2;
  }
  free(ctx.acc.p);
  return rc;
}

static const luaL_Reg llm_funcs[] = {
    {"ask", l_ask},
    {NULL, NULL},
};

/* require("llm") 進入點 */
int luaopen_llm(lua_State *L) {
  luaL_newlib(L, llm_funcs);
  return 1;
}
