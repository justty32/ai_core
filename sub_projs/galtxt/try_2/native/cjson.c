/*
** cjson.c — galtxt try_2：native（C）JSON codec，取代 vendored rxi/json.lua。
**
** 對齊 rxi/json.lua 0.1.2 的語意（測試照它寫的，必須一致）：
**   encode：1..n 序列 table→JSON 陣列、其餘字串鍵 table→object、空表→[]；
**           中文（UTF-8 高位元組）原樣輸出、不 \u 跳脫；false 如實；
**           數字整數印整數、浮點 %.14g；字串跳脫 " \ 與 0x00–0x1f。
**   decode：object→table（string 鍵）、array→1-index 序列、null→nil（object 該鍵消失）；
**           字串支援 \" \\ \/ \b \f \n \r \t \uXXXX（含 surrogate pair）→UTF-8；
**           數字整值回 Lua integer；尾端多餘字元報錯。
**
** ★ 這檔編進 liblua.a，經 linit.c 註冊進 package.preload，host.exe 與 stock lua.exe
**   兩者皆 require("cjson") 即得（都走 luaL_openselectedlibs）。無 DLL、Windows/Linux 一致。
**
** 實作要點：
**   encode 用「單一 heap 緩衝」逐層寫入，不用 luaL_Buffer——因為 Lua 手冊明訂 buffer
**   操作之間堆疊須保持平衡，而遞迴遍歷 table（rawgeti/next 把值留在堆疊上）做不到；
**   單一共用緩衝在錯誤路徑一次 free 後 luaL_error，各遞迴層不各自配置、無洩漏。
**   decode 的字串用 luaL_Buffer（葉節點、與其它堆疊操作隔離，完全平衡、安全）。
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define CJSON_MAX_DEPTH 500  /* 遞迴深度上限，杜絕惡意/畸形深巢狀吃爆 C 堆疊 */

/* ─────────────────────────── Encode ─────────────────────────── */

/* 成長型 heap 緩衝（整趟 encode 共用一份） */
typedef struct { char *b; size_t len, cap; } Buf;

static void buf_reserve(lua_State *L, Buf *B, size_t add) {
  if (B->len + add <= B->cap) return;
  size_t nc = B->cap ? B->cap : 256;
  while (nc < B->len + add) nc *= 2;
  char *nb = (char *)realloc(B->b, nc);
  if (!nb) { free(B->b); luaL_error(L, "cjson.encode: 記憶體不足"); }
  B->b = nb; B->cap = nc;
}
static void buf_addlstr(lua_State *L, Buf *B, const char *s, size_t n) {
  buf_reserve(L, B, n); memcpy(B->b + B->len, s, n); B->len += n;
}
static void buf_addch(lua_State *L, Buf *B, char c) {
  buf_reserve(L, B, 1); B->b[B->len++] = c;
}
#define BUF_LIT(L, B, lit) buf_addlstr((L), (B), "" lit, sizeof(lit) - 1)

static void encode_string(lua_State *L, Buf *B, const char *s, size_t n) {
  buf_addch(L, B, '"');
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    switch (c) {
      case '"':  BUF_LIT(L, B, "\\\""); break;
      case '\\': BUF_LIT(L, B, "\\\\"); break;
      case '\b': BUF_LIT(L, B, "\\b");  break;
      case '\f': BUF_LIT(L, B, "\\f");  break;
      case '\n': BUF_LIT(L, B, "\\n");  break;
      case '\r': BUF_LIT(L, B, "\\r");  break;
      case '\t': BUF_LIT(L, B, "\\t");  break;
      default:
        if (c < 0x20) {  /* 其餘控制字元→\u00xx（≥0x20 含 UTF-8 高位元組原樣） */
          char u[8]; int k = snprintf(u, sizeof u, "\\u%04x", c);
          buf_addlstr(L, B, u, (size_t)k);
        } else {
          buf_addch(L, B, (char)c);
        }
    }
  }
  buf_addch(L, B, '"');
}

static void encode_value(lua_State *L, int idx, Buf *B, int depth);

static void encode_table(lua_State *L, int idx, Buf *B, int depth) {
  if (depth > CJSON_MAX_DEPTH) { free(B->b); luaL_error(L, "cjson.encode: 巢狀過深"); }

  /* 陣列判定＝rxi：有 [1] 或整體為空 → 當陣列 */
  lua_rawgeti(L, idx, 1);
  int has1 = !lua_isnil(L, -1);
  lua_pop(L, 1);
  lua_pushnil(L);
  int is_empty = (lua_next(L, idx) == 0);
  if (!is_empty) lua_pop(L, 2);  /* 丟棄探測用的 key,val */
  int is_array = has1 || is_empty;

  if (is_array) {
    buf_addch(L, B, '[');
    lua_Integer n = (lua_Integer)lua_rawlen(L, idx);
    for (lua_Integer i = 1; i <= n; i++) {
      if (i > 1) buf_addch(L, B, ',');
      lua_rawgeti(L, idx, i);
      encode_value(L, lua_gettop(L), B, depth + 1);
      lua_pop(L, 1);
    }
    buf_addch(L, B, ']');
  } else {
    buf_addch(L, B, '{');
    lua_pushnil(L);
    int first = 1;
    while (lua_next(L, idx) != 0) {           /* key 在 -2、value 在 -1 */
      if (lua_type(L, -2) != LUA_TSTRING) {
        lua_pop(L, 2); free(B->b);
        luaL_error(L, "cjson.encode: object 鍵必須是字串");
      }
      if (!first) buf_addch(L, B, ',');
      first = 0;
      size_t klen; const char *k = lua_tolstring(L, -2, &klen);  /* 鍵確為字串，安全 */
      encode_string(L, B, k, klen);
      buf_addch(L, B, ':');
      encode_value(L, lua_gettop(L), B, depth + 1);              /* value 在 top */
      lua_pop(L, 1);                                             /* 丟 value、留 key 續走 */
    }
    buf_addch(L, B, '}');
  }
}

static void encode_value(lua_State *L, int idx, Buf *B, int depth) {
  switch (lua_type(L, idx)) {
    case LUA_TNIL:
      BUF_LIT(L, B, "null");
      break;
    case LUA_TBOOLEAN:
      if (lua_toboolean(L, idx)) BUF_LIT(L, B, "true");
      else                       BUF_LIT(L, B, "false");
      break;
    case LUA_TNUMBER: {
      char num[48];
      int k;
      if (lua_isinteger(L, idx))
        k = snprintf(num, sizeof num, "%lld", (long long)lua_tointeger(L, idx));
      else {
        double d = (double)lua_tonumber(L, idx);
        if (isnan(d) || isinf(d)) { free(B->b); luaL_error(L, "cjson.encode: 數值非有限（NaN/Inf）"); }
        k = snprintf(num, sizeof num, "%.14g", d);
      }
      buf_addlstr(L, B, num, (size_t)k);
      break;
    }
    case LUA_TSTRING: {
      size_t n; const char *s = lua_tolstring(L, idx, &n);
      encode_string(L, B, s, n);
      break;
    }
    case LUA_TTABLE:
      encode_table(L, idx, B, depth);
      break;
    default:
      free(B->b);
      luaL_error(L, "cjson.encode: 無法編碼的型別 '%s'", luaL_typename(L, idx));
  }
}

static int cjson_encode(lua_State *L) {
  luaL_checkany(L, 1);
  Buf B = { NULL, 0, 0 };
  encode_value(L, 1, &B, 0);
  lua_pushlstring(L, B.b ? B.b : "", B.len);
  free(B.b);
  return 1;
}

/* ─────────────────────────── Decode ─────────────────────────── */

typedef struct { lua_State *L; const char *s; size_t len, pos; int depth; } PS;

static void ps_error(PS *p, const char *msg) {
  /* 算出行列，訊息對齊 rxi 風格 */
  int line = 1, col = 1;
  for (size_t i = 0; i < p->pos && i < p->len; i++) {
    if (p->s[i] == '\n') { line++; col = 1; } else col++;
  }
  luaL_error(p->L, "cjson.decode: %s at line %d col %d", msg, line, col);
}

static void skip_ws(PS *p) {
  while (p->pos < p->len) {
    char c = p->s[p->pos];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') p->pos++;
    else break;
  }
}

static void parse_value(PS *p);

static int hex4(PS *p) {  /* 讀 p->pos 起 4 個十六進位 → 碼位；失敗回 -1，成功後 pos 前進 4 */
  if (p->pos + 4 > p->len) return -1;
  int v = 0;
  for (int i = 0; i < 4; i++) {
    char c = p->s[p->pos + i];
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return -1;
    v = v * 16 + d;
  }
  p->pos += 4;
  return v;
}

static void add_utf8(luaL_Buffer *b, long cp) {
  if (cp <= 0x7f) {
    luaL_addchar(b, (char)cp);
  } else if (cp <= 0x7ff) {
    luaL_addchar(b, (char)(0xc0 | (cp >> 6)));
    luaL_addchar(b, (char)(0x80 | (cp & 0x3f)));
  } else if (cp <= 0xffff) {
    luaL_addchar(b, (char)(0xe0 | (cp >> 12)));
    luaL_addchar(b, (char)(0x80 | ((cp >> 6) & 0x3f)));
    luaL_addchar(b, (char)(0x80 | (cp & 0x3f)));
  } else {
    luaL_addchar(b, (char)(0xf0 | (cp >> 18)));
    luaL_addchar(b, (char)(0x80 | ((cp >> 12) & 0x3f)));
    luaL_addchar(b, (char)(0x80 | ((cp >> 6) & 0x3f)));
    luaL_addchar(b, (char)(0x80 | (cp & 0x3f)));
  }
}

static void parse_string(PS *p) {  /* 入口時 p->s[p->pos]=='"'；結束把字串推上堆疊 */
  p->pos++;  /* 跳開頭引號 */
  luaL_Buffer b;
  luaL_buffinit(p->L, &b);
  while (p->pos < p->len) {
    unsigned char c = (unsigned char)p->s[p->pos];
    if (c == '"') { p->pos++; luaL_pushresult(&b); return; }
    if (c == '\\') {
      p->pos++;
      if (p->pos >= p->len) break;
      char e = p->s[p->pos++];
      switch (e) {
        case '"':  luaL_addchar(&b, '"');  break;
        case '\\': luaL_addchar(&b, '\\'); break;
        case '/':  luaL_addchar(&b, '/');  break;
        case 'b':  luaL_addchar(&b, '\b'); break;
        case 'f':  luaL_addchar(&b, '\f'); break;
        case 'n':  luaL_addchar(&b, '\n'); break;
        case 'r':  luaL_addchar(&b, '\r'); break;
        case 't':  luaL_addchar(&b, '\t'); break;
        case 'u': {
          int hi = hex4(p);
          if (hi < 0) { p->pos--; ps_error(p, "invalid \\u escape"); }
          long cp = hi;
          if (hi >= 0xd800 && hi <= 0xdbff) {  /* 高代理，找隨後的 \uXXXX 低代理 */
            if (p->pos + 2 <= p->len && p->s[p->pos] == '\\' && p->s[p->pos + 1] == 'u') {
              size_t save = p->pos;
              p->pos += 2;
              int lo = hex4(p);
              if (lo >= 0xdc00 && lo <= 0xdfff)
                cp = 0x10000 + ((long)(hi - 0xd800) << 10) + (lo - 0xdc00);
              else
                p->pos = save;  /* 不成對，退回、hi 單獨編碼 */
            }
          }
          add_utf8(&b, cp);
          break;
        }
        default: p->pos--; ps_error(p, "invalid escape char in string");
      }
    } else if (c < 0x20) {
      ps_error(p, "control character in string");
    } else {
      luaL_addchar(&b, (char)c);  /* 含 UTF-8 高位元組原樣 */
      p->pos++;
    }
  }
  ps_error(p, "expected closing quote for string");
}

static void parse_number(PS *p) {
  size_t start = p->pos;
  while (p->pos < p->len) {
    char c = p->s[p->pos];
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') p->pos++;
    else break;
  }
  size_t n = p->pos - start;
  char tmp[64];
  if (n == 0 || n >= sizeof tmp) { p->pos = start; ps_error(p, "invalid number"); }
  memcpy(tmp, p->s + start, n);
  tmp[n] = '\0';
  /* lua_stringtonumber：整值→integer、否則→float，與 Lua 語意一致 */
  if (lua_stringtonumber(p->L, tmp) == 0) { p->pos = start; ps_error(p, "invalid number"); }
}

static int match_lit(PS *p, const char *word) {
  size_t n = strlen(word);
  if (p->pos + n <= p->len && memcmp(p->s + p->pos, word, n) == 0) { p->pos += n; return 1; }
  return 0;
}

static void parse_array(PS *p) {
  if (++p->depth > CJSON_MAX_DEPTH) ps_error(p, "nesting too deep");
  p->pos++;  /* 跳 '[' */
  lua_newtable(p->L);
  int tab = lua_gettop(p->L);
  lua_Integer i = 0;
  skip_ws(p);
  if (p->pos < p->len && p->s[p->pos] == ']') { p->pos++; p->depth--; return; }
  for (;;) {
    parse_value(p);                      /* 值推上 top */
    lua_rawseti(p->L, tab, ++i);         /* t[i]=value（值為 nil 則成空洞，同 rxi） */
    skip_ws(p);
    if (p->pos >= p->len) ps_error(p, "expected ']' or ','");
    char c = p->s[p->pos++];
    if (c == ']') break;
    if (c != ',') ps_error(p, "expected ']' or ','");
    skip_ws(p);
  }
  p->depth--;
}

static void parse_object(PS *p) {
  if (++p->depth > CJSON_MAX_DEPTH) ps_error(p, "nesting too deep");
  p->pos++;  /* 跳 '{' */
  lua_newtable(p->L);
  int tab = lua_gettop(p->L);
  skip_ws(p);
  if (p->pos < p->len && p->s[p->pos] == '}') { p->pos++; p->depth--; return; }
  for (;;) {
    skip_ws(p);
    if (p->pos >= p->len || p->s[p->pos] != '"') ps_error(p, "expected string key");
    parse_string(p);                     /* key 推上 top */
    skip_ws(p);
    if (p->pos >= p->len || p->s[p->pos] != ':') ps_error(p, "expected ':' after key");
    p->pos++;
    parse_value(p);                      /* value 推上 top（key 在 -2、tab 在下） */
    lua_rawset(p->L, tab);               /* t[key]=value；value 為 nil 則該鍵不設，同 rxi */
    skip_ws(p);
    if (p->pos >= p->len) ps_error(p, "expected '}' or ','");
    char c = p->s[p->pos++];
    if (c == '}') break;
    if (c != ',') ps_error(p, "expected '}' or ','");
  }
  p->depth--;
}

static void parse_value(PS *p) {
  skip_ws(p);
  if (p->pos >= p->len) ps_error(p, "unexpected end of input");
  char c = p->s[p->pos];
  switch (c) {
    case '{': parse_object(p); break;
    case '[': parse_array(p);  break;
    case '"': parse_string(p); break;
    case 't': if (!match_lit(p, "true"))  ps_error(p, "invalid literal"); lua_pushboolean(p->L, 1); break;
    case 'f': if (!match_lit(p, "false")) ps_error(p, "invalid literal"); lua_pushboolean(p->L, 0); break;
    case 'n': if (!match_lit(p, "null"))  ps_error(p, "invalid literal"); lua_pushnil(p->L); break;
    default:
      if (c == '-' || (c >= '0' && c <= '9')) parse_number(p);
      else ps_error(p, "unexpected character");
  }
}

static int cjson_decode(lua_State *L) {
  size_t len;
  const char *s = luaL_checklstring(L, 1, &len);
  PS p = { L, s, len, 0, 0 };
  parse_value(&p);
  skip_ws(&p);
  if (p.pos < p.len) ps_error(&p, "trailing garbage");
  return 1;
}

/* ─────────────────────────── 模組登記 ─────────────────────────── */

static const luaL_Reg cjson_funcs[] = {
  { "encode", cjson_encode },
  { "decode", cjson_decode },
  { NULL, NULL }
};

LUAMOD_API int luaopen_cjson(lua_State *L) {
  luaL_newlib(L, cjson_funcs);
  lua_pushliteral(L, "0.1.2-c");
  lua_setfield(L, -2, "_version");
  return 1;
}
