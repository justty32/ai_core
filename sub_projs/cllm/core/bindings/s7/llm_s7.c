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
 * 範圍：prompt＋連線/取樣選項＋stream＋schema＋on-delta/on-error＋tools/media/modalities。
 *
 * ── 新增 keyword（皆為 list，形狀對齊 llm_tool_def_t / llm_media_in_t / llm_modality_t 的欄位順序）──
 *   :tools     = list of (list name description parameters-json)          ; 送給模型看的工具定義
 *   :on-tool   = (lambda (id name arguments) …)                            ; 模型要求呼叫工具，回 #t 中止
 *   :media     = list of (list url mime data)                              ; 輸入媒體（圖／音），mime/data 可 #f
 *   :modalities= list of (cons name config-json)                           ; 想要的輸出模態＋參數（config 可 #f）
 *   :on-media  = (lambda (mime bytes) …)                                   ; 模型產出的媒體，回 #t 中止
 *
 * ⚠⚠ 回呼安全鐵則：on-delta/on-error/on-tool/on-media 全部會在 llm_ask（C++ 實作）執行途中被呼叫，
 *   s7 error 以 longjmp 實作，longjmp 穿過 C++ 堆疊是未定義行為。回呼請只做無害的事
 *   （display／存值等），絕不能在回呼裡讓 s7 拋 error（包含型別錯誤、未綁定變數等）。
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
  s7_pointer on_delta, on_error, on_tool, on_media; /* NULL＝未給 */
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
/* 模型要求呼叫工具：(on-tool id name arguments)，回 #t 中止。永遠拼完整才交付（不受 stream 影響）。*/
static int on_tool_cb(const llm_tool_call_t *call, void *u) {
  sctx *c = (sctx *)u;
  if (!c->on_tool) return 0;
  s7_pointer res = s7_call(
      c->sc, c->on_tool,
      s7_list(c->sc, 3,
              s7_make_string(c->sc, call->id ? call->id : ""),
              s7_make_string(c->sc, call->name ? call->name : ""),
              s7_make_string(c->sc, call->arguments ? call->arguments : "")));
  return (res == s7_t(c->sc)) ? 1 : 0;
}
/* 模型產出的媒體（如 audio）：(on-media mime bytes)，回 #t 中止。bytes 是原始位元組（非 NUL 結尾保證）。*/
static int on_media_cb(const llm_media_out_t *media, void *u) {
  sctx *c = (sctx *)u;
  if (!c->on_media) return 0;
  s7_pointer res = s7_call(
      c->sc, c->on_media,
      s7_list(c->sc, 2,
              s7_make_string(c->sc, media->mime ? media->mime : ""),
              s7_make_string_with_length(c->sc, media->data ? media->data : "", (s7_int)media->len)));
  return (res == s7_t(c->sc)) ? 1 : 0;
}

/* 算 s7 list 長度（只走 pair 鏈，非 list 或空 list 回 0）。*/
static size_t s7_list_count(s7_pointer lst) {
  size_t n = 0;
  for (s7_pointer p = lst; s7_is_pair(p); p = s7_cdr(p)) n++;
  return n;
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
  /* :tools/:media/:modalities 的 C 陣列由這裡 malloc；元素字串都指回 s7 物件內部，
   * 只要 args（連帶 :tools 等 list）在 llm_ask 期間活著即可，陣列本身在函式結尾統一釋放。*/
  llm_tool_def_t *tools = NULL; size_t tools_n = 0;
  llm_media_in_t *media = NULL; size_t media_n = 0;
  llm_modality_t *modalities = NULL; size_t modalities_n = 0;

  s7_pointer rest = s7_cdr(args);
  if (s7_is_pair(rest)) {
    s7_pointer first = s7_car(rest);
    if (s7_is_keyword(first)) { /* keyword 選項形式 */
      s7_pointer v;
      if ((v = kw_get(sc, rest, "endpoint"))    && s7_is_string(v))  c.endpoint = s7_string(v);
      if ((v = kw_get(sc, rest, "system"))      && s7_is_string(v))  r.system   = s7_string(v); /* system role 指示 */
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
      if ((v = kw_get(sc, rest, "on-tool"))     && s7_is_procedure(v)) ctx.on_tool  = v;
      if ((v = kw_get(sc, rest, "on-media"))    && s7_is_procedure(v)) ctx.on_media = v;

      /* :tools = list of (list name description parameters-json) */
      if ((v = kw_get(sc, rest, "tools")) && s7_is_pair(v)) {
        tools_n = s7_list_count(v);
        tools = (llm_tool_def_t *)calloc(tools_n, sizeof *tools);
        size_t i = 0;
        for (s7_pointer p = v; s7_is_pair(p); p = s7_cdr(p), i++) {
          /* item 須逐層確認是 pair 才能取 car/cdr——s7_car/s7_cdr 對非 pair 直接讀記憶體，型別不對會炸。*/
          s7_pointer item = s7_car(p);
          s7_pointer nm = NULL, ds = NULL, pr = NULL;
          if (s7_is_pair(item)) {
            nm = s7_car(item);
            s7_pointer t1 = s7_cdr(item);
            if (s7_is_pair(t1)) {
              ds = s7_car(t1);
              s7_pointer t2 = s7_cdr(t1);
              if (s7_is_pair(t2)) pr = s7_car(t2);
            }
          }
          tools[i].name        = (nm && s7_is_string(nm)) ? s7_string(nm) : "";
          tools[i].description = (ds && s7_is_string(ds)) ? s7_string(ds) : "";
          tools[i].parameters  = (pr && s7_is_string(pr)) ? s7_string(pr) : "{}";
        }
        r.tools = tools; r.tools_count = tools_n;
      }

      /* :media = list of (list url mime data)；mime/data 可省或給 #f */
      if ((v = kw_get(sc, rest, "media")) && s7_is_pair(v)) {
        media_n = s7_list_count(v);
        media = (llm_media_in_t *)calloc(media_n, sizeof *media);
        size_t i = 0;
        for (s7_pointer p = v; s7_is_pair(p); p = s7_cdr(p), i++) {
          s7_pointer item = s7_car(p);
          s7_pointer u = NULL, mm = NULL, dt = NULL;
          if (s7_is_pair(item)) {
            u = s7_car(item);
            s7_pointer t1 = s7_cdr(item);
            if (s7_is_pair(t1)) {
              mm = s7_car(t1);
              s7_pointer t2 = s7_cdr(t1);
              if (s7_is_pair(t2)) dt = s7_car(t2);
            }
          }
          media[i].url  = (u  && s7_is_string(u))  ? s7_string(u)  : NULL;
          media[i].mime = (mm && s7_is_string(mm)) ? s7_string(mm) : NULL;
          if (dt && s7_is_string(dt)) { media[i].data = s7_string(dt); media[i].len = (size_t)s7_string_length(dt); }
        }
        r.media = media; r.media_count = media_n;
      }

      /* :modalities = list of (cons name config-json)；config 可省或給 #f */
      if ((v = kw_get(sc, rest, "modalities")) && s7_is_pair(v)) {
        modalities_n = s7_list_count(v);
        modalities = (llm_modality_t *)calloc(modalities_n, sizeof *modalities);
        size_t i = 0;
        for (s7_pointer p = v; s7_is_pair(p); p = s7_cdr(p), i++) {
          s7_pointer item = s7_car(p);
          s7_pointer nm = NULL, cf = NULL;
          if (s7_is_pair(item)) {
            nm = s7_car(item);
            cf = s7_cdr(item); /* 點對：cdr 就是 config 值本身（proper list 時 cdr 是子 list，非字串會被下面濾掉）*/
          }
          modalities[i].name   = (nm && s7_is_string(nm)) ? s7_string(nm) : "";
          modalities[i].config = (cf && s7_is_string(cf)) ? s7_string(cf) : NULL;
        }
        r.modalities = modalities; r.modalities_count = modalities_n;
      }
    } else if (s7_is_string(first)) { /* 位置形式：第二參數＝endpoint */
      c.endpoint = s7_string(first);
    }
  }

  llm_handlers_t h; memset(&h, 0, sizeof h);
  h.on_text  = on_text_cb;  h.text_user  = &ctx;
  h.on_error = on_error_cb; h.error_user = &ctx;
  h.on_tool  = on_tool_cb;  h.tool_user  = &ctx;
  h.on_media = on_media_cb; h.media_user = &ctx;

  llm_status_t st = llm_ask(NULL, &c, &r, &h);

  s7_pointer result;
  if (st == LLM_OK) {
    result = s7_make_string_with_length(sc, ctx.acc.p ? ctx.acc.p : "", (s7_int)ctx.acc.len);
  } else if (ctx.on_error) {
    result = s7_f(sc); /* on-error 已被呼叫 */
  } else {
    char msg[512]; snprintf(msg, sizeof msg, "%s", ctx.errmsg[0] ? ctx.errmsg : "llm_ask failed");
    free(ctx.acc.p); free(tools); free(media); free(modalities);
    return s7_error(sc, s7_make_symbol(sc, "llm-error"), s7_list(sc, 1, s7_make_string(sc, msg)));
  }
  free(ctx.acc.p); free(tools); free(media); free(modalities);
  return result;
}

/* 註冊到既有的 s7 直譯器（自帶 host 或嵌入端皆可呼叫）。*/
void llm_s7_init(s7_scheme *sc) {
  s7_define_function(
      sc, "llm-ask", g_llm_ask, 1, 0, true,
      "(llm-ask prompt [endpoint] | prompt :endpoint s :model s :temperature x :stream #t "
      ":on-delta proc :on-error proc :schema json "
      ":tools ((name desc params-json) …) :on-tool proc "
      ":media ((url mime data) …) :modalities ((name . config-json) …) :on-media proc …) "
      "透過 libcllm 問 LLM，回完整答案字串");
}
