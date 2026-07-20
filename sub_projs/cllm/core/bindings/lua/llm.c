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
 * 消費穩定 C ABI（見 ../../docs/c-abi-input.md、../../docs/c-abi-output.md）；只連 -lcllm，不需
 * cllm 的建置環境。
 * 範圍：prompt＋連線/取樣選項＋stream＋schema（結構化輸出）＋on_delta/on_error＋
 *   tools/on_tool（工具定義／工具呼叫回呼）＋media/on_media（輸入媒體／產出媒體回呼）＋
 *   modalities（想要的輸出模態）。opts 欄位皆 snake_case，詳見 ../README.md。
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
  int on_tool_ref, on_media_ref;  /* 同上，工具呼叫／產出媒體回呼 */
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

/* 工具呼叫：模型每要求一個工具呼一次，交回 {id=, name=, arguments=}（arguments 是 JSON 字串，
 * 解析交給呼叫端，例如用 dkjson）。回呼回 true＝中止（不執行其餘工具呼叫）。*/
static int on_tool_cb(const llm_tool_call_t *call, void *u) {
  lctx *c = (lctx *)u;
  if (c->on_tool_ref == LUA_NOREF) return 0;
  lua_State *L = c->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, c->on_tool_ref);
  lua_newtable(L);
  lua_pushstring(L, call->id ? call->id : "");        lua_setfield(L, -2, "id");
  lua_pushstring(L, call->name ? call->name : "");    lua_setfield(L, -2, "name");
  lua_pushstring(L, call->arguments ? call->arguments : ""); lua_setfield(L, -2, "arguments");
  if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
    snprintf(c->errmsg, sizeof c->errmsg, "on_tool 回呼出錯：%s", lua_tostring(L, -1));
    lua_pop(L, 1);
    c->cb_error = 1;
    return 1; /* 中止 */
  }
  int abort = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return abort;
}

/* 產出媒體：模型每產出一則媒體（如 audio）呼一次，交回 {mime=, bytes=}（bytes 是二進位字串，
 * Lua string 天生二進位安全，長度看 #m.bytes）。回呼回 true＝中止。*/
static int on_media_cb(const llm_media_out_t *media, void *u) {
  lctx *c = (lctx *)u;
  if (c->on_media_ref == LUA_NOREF) return 0;
  lua_State *L = c->L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, c->on_media_ref);
  lua_newtable(L);
  lua_pushstring(L, media->mime ? media->mime : "");            lua_setfield(L, -2, "mime");
  lua_pushlstring(L, media->data ? media->data : "", media->len); lua_setfield(L, -2, "bytes");
  if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
    snprintf(c->errmsg, sizeof c->errmsg, "on_media 回呼出錯：%s", lua_tostring(L, -1));
    lua_pop(L, 1);
    c->cb_error = 1;
    return 1; /* 中止 */
  }
  int abort = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return abort;
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
/* 欄位是二進位字串（如 media 的 bytes）→ malloc 一份拷貝（原字串可能被 Lua GC 收走），
 * 存 len；欄位缺/非字串則回 NULL、*len=0。*/
static char *dup_bytes_field(lua_State *L, int t, const char *k, size_t *len) {
  lua_getfield(L, t, k);
  char *r = NULL; *len = 0;
  if (lua_isstring(L, -1)) {
    size_t n; const char *s = lua_tolstring(L, -1, &n);
    r = (char *)malloc(n ? n : 1);
    memcpy(r, s, n);
    *len = n;
  }
  lua_pop(L, 1);
  return r;
}

/* ── opts.tools／opts.media／opts.modalities：array-of-table → heap 陣列 ──
 * 各 build_* 回傳的陣列與其內字串欄位都是 heap 配置，call 完要用對應 free_* 釋放。 */
static llm_tool_def_t *build_tools(lua_State *L, int optidx, size_t *count) {
  lua_getfield(L, optidx, "tools");
  if (!lua_istable(L, -1)) { lua_pop(L, 1); *count = 0; return NULL; }
  size_t n = lua_rawlen(L, -1);
  llm_tool_def_t *arr = n ? (llm_tool_def_t *)calloc(n, sizeof *arr) : NULL;
  for (size_t i = 0; i < n; i++) {
    lua_rawgeti(L, -1, (lua_Integer)(i + 1));
    int t = lua_gettop(L);
    arr[i].name        = dup_field(L, t, "name");
    arr[i].description = dup_field(L, t, "description");
    arr[i].parameters  = dup_field(L, t, "parameters");
    lua_pop(L, 1);
  }
  lua_pop(L, 1); /* tools table */
  *count = n;
  return arr;
}
static void free_tools(llm_tool_def_t *arr, size_t n) {
  for (size_t i = 0; i < n; i++) {
    free((void *)arr[i].name); free((void *)arr[i].description); free((void *)arr[i].parameters);
  }
  free(arr);
}

static llm_media_in_t *build_media_in(lua_State *L, int optidx, size_t *count) {
  lua_getfield(L, optidx, "media");
  if (!lua_istable(L, -1)) { lua_pop(L, 1); *count = 0; return NULL; }
  size_t n = lua_rawlen(L, -1);
  llm_media_in_t *arr = n ? (llm_media_in_t *)calloc(n, sizeof *arr) : NULL;
  for (size_t i = 0; i < n; i++) {
    lua_rawgeti(L, -1, (lua_Integer)(i + 1));
    int t = lua_gettop(L);
    arr[i].url  = dup_field(L, t, "url");
    arr[i].mime = dup_field(L, t, "mime");
    arr[i].data = dup_bytes_field(L, t, "bytes", &arr[i].len);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  *count = n;
  return arr;
}
static void free_media_in(llm_media_in_t *arr, size_t n) {
  for (size_t i = 0; i < n; i++) { free((void *)arr[i].url); free((void *)arr[i].mime); free((void *)arr[i].data); }
  free(arr);
}

static llm_modality_t *build_modalities(lua_State *L, int optidx, size_t *count) {
  lua_getfield(L, optidx, "modalities");
  if (!lua_istable(L, -1)) { lua_pop(L, 1); *count = 0; return NULL; }
  size_t n = lua_rawlen(L, -1);
  llm_modality_t *arr = n ? (llm_modality_t *)calloc(n, sizeof *arr) : NULL;
  for (size_t i = 0; i < n; i++) {
    lua_rawgeti(L, -1, (lua_Integer)(i + 1));
    int t = lua_gettop(L);
    arr[i].name   = dup_field(L, t, "name");
    arr[i].config = dup_field(L, t, "config");
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  *count = n;
  return arr;
}
static void free_modalities(llm_modality_t *arr, size_t n) {
  for (size_t i = 0; i < n; i++) { free((void *)arr[i].name); free((void *)arr[i].config); }
  free(arr);
}

/* llm.ask(prompt [, endpoint])｜llm.ask(prompt, opts)｜llm.ask(opts)
 *   → text（成功）｜ nil, errmsg（失敗/取消）*/
static int l_ask(lua_State *L) {
  int optidx = 0;               /* opts table 的堆疊位置，0＝無 */
  char *prompt_dup = NULL;      /* table 形式時 strdup 的 prompt */
  const char *prompt;
  char *endpoint = NULL, *api_key = NULL, *model = NULL, *schema_json = NULL, *system = NULL;
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
  ctx.on_tool_ref = LUA_NOREF; ctx.on_media_ref = LUA_NOREF;
  llm_tool_def_t *tools = NULL;       size_t tools_n = 0;
  llm_media_in_t *media = NULL;       size_t media_n = 0;
  llm_modality_t *modalities = NULL;  size_t modalities_n = 0;

  if (optidx) {
    endpoint = dup_field(L, optidx, "endpoint"); /* NULL = 內建 localhost */
    system   = dup_field(L, optidx, "system");   /* system role 指示；NULL = 不送 */
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
    ctx.on_tool_ref  = ref_field(L, optidx, "on_tool");
    ctx.on_media_ref = ref_field(L, optidx, "on_media");
    tools      = build_tools(L, optidx, &tools_n);
    media      = build_media_in(L, optidx, &media_n);
    modalities = build_modalities(L, optidx, &modalities_n);
  }
  c.endpoint = endpoint ? endpoint : pos_endpoint;
  c.api_key = api_key;
  c.model = model;
  r.system = system;
  if (schema_json) { schema.name = "response"; schema.json = schema_json; r.schema = &schema; }
  r.tools = tools;           r.tools_count = tools_n;
  r.media = media;           r.media_count = media_n;
  r.modalities = modalities; r.modalities_count = modalities_n;

  llm_handlers_t h; memset(&h, 0, sizeof h);
  h.on_text  = on_text_cb;  h.text_user  = &ctx;
  h.on_error = on_error_cb; h.error_user = &ctx;
  h.on_tool  = on_tool_cb;  h.tool_user  = &ctx;
  h.on_media = on_media_cb; h.media_user = &ctx;

  llm_status_t st = llm_ask(NULL, &c, &r, &h);

  if (ctx.on_delta_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ctx.on_delta_ref);
  if (ctx.on_error_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ctx.on_error_ref);
  if (ctx.on_tool_ref  != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ctx.on_tool_ref);
  if (ctx.on_media_ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ctx.on_media_ref);
  free_tools(tools, tools_n);
  free_media_in(media, media_n);
  free_modalities(modalities, modalities_n);
  free(endpoint); free(api_key); free(model); free(schema_json); free(system); free(prompt_dup);

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
