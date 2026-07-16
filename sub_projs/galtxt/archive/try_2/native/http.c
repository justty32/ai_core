/*
** http.c — galtxt try_2：native（C）HTTP 傳輸，取代 Lua 端的 io.popen("curl …")＋暫存檔。
**
** 分層原則：C 是「笨管子」——只管 TLS＋HTTP round-trip、串流時逐塊把 raw bytes 吐回 Lua；
** SSE 拆框／UTF-8 分批／JSON 編解／ask 縫，全留在 Lua/Fennel（協定與語意在腳本層，改起來便宜，
** 且這支 C transport 語言中立、三線共用）。
**
** 平台（比照 host.cpp 的 #ifdef 分流）：
**   Windows：WinHTTP（系統內建、Schannel TLS，零額外依賴；連結 -lwinhttp）。
**   其它（Linux/Mac）：libcurl（連結 -lcurl）。
**   兩平台共用 file:// 特例：直接讀檔當 200 回應——保住離線 curl file:// fixture 測試 harness
**   （WinHTTP 不支援 file://，故非在 C 這層處理不可）。
**
** 這檔編進 liblua.a、經 linit.c 註冊進 package.preload，host.exe 與 stock lua.exe 皆 require("http") 即得。
**
** Lua API：
**   http.request{url=, method=?, headers=?{ "K: V", … }, body=?, timeout=?ms}
**       → status:int, body:str          （非串流；傳輸失敗會 raise）
**   http.stream {url=, …, on_data=function(bytes) end}
**       → status:int                    （內容經 on_data 逐塊餵；串流完成回 status）
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/* ── 成長型緩衝（非串流時累積回應本體） */
typedef struct { char *b; size_t len, cap; } Buf;
static void buf_append(lua_State *L, Buf *B, const char *p, size_t n) {
  if (B->len + n > B->cap) {
    size_t nc = B->cap ? B->cap : 4096;
    while (nc < B->len + n) nc *= 2;
    char *nb = (char *)realloc(B->b, nc);
    if (!nb) { free(B->b); B->b = NULL; luaL_error(L, "http: 記憶體不足"); }
    B->b = nb; B->cap = nc;
  }
  memcpy(B->b + B->len, p, n); B->len += n;
}

/* ── 傳輸情境：非串流累積到 buf；串流則呼叫 Lua on_data（在 cb_idx） */
typedef struct {
  lua_State *L;
  int stream;    /* bool */
  int cb_idx;    /* on_data 在堆疊的絕對位置（串流用） */
  Buf buf;
  int aborted;   /* on_data 呼叫失敗→中止 */
} Ctx;

/* 把一塊收到的 bytes 交付：串流→呼 on_data；非串流→累積。回傳是否應繼續（0＝中止）。 */
static int deliver(Ctx *c, const char *p, size_t n) {
  if (c->aborted) return 0;
  if (c->stream) {
    lua_pushvalue(c->L, c->cb_idx);          /* on_data */
    lua_pushlstring(c->L, p, n);
    if (lua_pcall(c->L, 1, 0, 0) != LUA_OK) { c->aborted = 1; return 0; }  /* 錯誤物件留在堆疊頂 */
    return 1;
  }
  buf_append(c->L, &c->buf, p, n);
  return 1;
}

/* ── opts 讀取小工具 */
static const char *opt_str(lua_State *L, int t, const char *k, const char *dflt, size_t *len) {
  lua_getfield(L, t, k);
  const char *s = dflt;
  if (lua_isstring(L, -1)) s = lua_tolstring(L, -1, len);
  else if (len && dflt) *len = strlen(dflt);
  lua_pop(L, 1);   /* 值已在上面轉存指標；注意：字串生命週期靠 opts table 仍在堆疊上撐著 */
  return s;
}

/* ── file:// 特例：讀整個檔當回應本體（200）。回傳 status（讀不到→raise）。 */
static int do_file(lua_State *L, const char *url, Ctx *c) {
  const char *path = url + 7;               /* 跳過 "file://" */
  /* Windows 的 "file:///C:/…" 去掉開頭那個 '/'；Linux 的 "/home/…" 保留 */
  if (path[0] == '/' && path[1] && path[2] == ':') path += 1;
  FILE *f = fopen(path, "rb");
  if (!f) luaL_error(L, "http: 開不了 file:// 路徑：%s", path);
  char tmp[8192]; size_t r;
  while ((r = fread(tmp, 1, sizeof tmp, f)) > 0) {
    if (!deliver(c, tmp, r)) break;
  }
  fclose(f);
  return 200;
}

/* ══════════════════════════ 平台實作 ══════════════════════════ */
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>

static wchar_t *u8_to_w(const char *s, int len) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s, len, NULL, 0);
  wchar_t *w = (wchar_t *)malloc((size_t)(n + 1) * sizeof(wchar_t));
  MultiByteToWideChar(CP_UTF8, 0, s, len, w, n);
  w[n] = 0;
  return w;
}

/* 一次 HTTP 交易（WinHTTP）。headers_joined＝以 \r\n 串接的所有標頭（UTF-8），可為 NULL。 */
static int do_http(lua_State *L, const char *url, const char *method,
                   const char *body, size_t blen,
                   const char *headers_joined, long timeout_ms, Ctx *c) {
  wchar_t *wurl = u8_to_w(url, -1);
  URL_COMPONENTS uc; wchar_t host[256], path[8192];
  memset(&uc, 0, sizeof uc); uc.dwStructSize = sizeof uc;
  uc.lpszHostName = host; uc.dwHostNameLength = 256;
  uc.lpszUrlPath = path; uc.dwUrlPathLength = 8192;
  if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) { free(wurl); luaL_error(L, "http: URL 解析失敗：%s", url); }

  HINTERNET hS = WinHttpOpen(L"galtxt-try2/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hS) { free(wurl); luaL_error(L, "http: WinHttpOpen 失敗（%lu）", GetLastError()); }
  if (timeout_ms > 0) WinHttpSetTimeouts(hS, timeout_ms, timeout_ms, timeout_ms, timeout_ms);

  HINTERNET hC = WinHttpConnect(hS, host, uc.nPort, 0);
  wchar_t *wmethod = u8_to_w(method, -1);
  DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET hR = hC ? WinHttpOpenRequest(hC, wmethod, path, NULL, WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : NULL;
  free(wmethod); free(wurl);
  if (!hR) {
    if (hC) WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
    luaL_error(L, "http: 建立請求失敗（%lu）", GetLastError());
  }
  if (headers_joined && *headers_joined) {
    wchar_t *wh = u8_to_w(headers_joined, -1);
    WinHttpAddRequestHeaders(hR, wh, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    free(wh);
  }
  BOOL ok = WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               (LPVOID)body, (DWORD)blen, (DWORD)blen, 0);
  if (ok) ok = WinHttpReceiveResponse(hR, NULL);
  if (!ok) {
    DWORD e = GetLastError();
    WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
    luaL_error(L, "http: 送出/接收失敗（%lu）——後端沒開？", e);
  }
  DWORD status = 0, sz = sizeof status;
  WinHttpQueryHeaders(hR, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(hR, &avail) || avail == 0) break;
    char *tmp = (char *)malloc(avail);
    DWORD got = 0;
    if (!WinHttpReadData(hR, tmp, avail, &got) || got == 0) { free(tmp); break; }
    int cont = deliver(c, tmp, got);
    free(tmp);
    if (!cont) break;
  }
  WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
  return (int)status;
}

#else  /* ── 非 Windows：libcurl */
#include <curl/curl.h>

static size_t curl_write(char *ptr, size_t size, size_t nmemb, void *ud) {
  Ctx *c = (Ctx *)ud;
  size_t n = size * nmemb;
  return deliver(c, ptr, n) ? n : 0;     /* 回傳 <n 會讓 libcurl 中止 */
}

static int do_http(lua_State *L, const char *url, const char *method,
                   const char *body, size_t blen,
                   const char *headers_joined, long timeout_ms, Ctx *c) {
  (void)headers_joined;  /* Linux 走 slist，見下；此參數僅 Windows 用 */
  CURL *h = curl_easy_init();
  if (!h) luaL_error(L, "http: curl_easy_init 失敗");
  curl_easy_setopt(h, CURLOPT_URL, url);
  if (strcmp(method, "POST") == 0) {
    curl_easy_setopt(h, CURLOPT_POSTFIELDS, body ? body : "");
    curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, (long)blen);
  } else {
    curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, method);
  }
  /* 標頭：libcurl 用 curl_slist（一條一個），由呼叫端在堆疊上的 headers 陣列現組 */
  struct curl_slist *hs = NULL;
  lua_getfield(L, 1, "headers");
  if (lua_istable(L, -1)) {
    lua_Integer n = (lua_Integer)lua_rawlen(L, -1);
    for (lua_Integer i = 1; i <= n; i++) {
      lua_rawgeti(L, -1, i);
      if (lua_isstring(L, -1)) hs = curl_slist_append(hs, lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);
  if (hs) curl_easy_setopt(h, CURLOPT_HTTPHEADER, hs);
  if (timeout_ms > 0) curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, timeout_ms);
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(h, CURLOPT_WRITEDATA, c);
  curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
  CURLcode rc = curl_easy_perform(h);
  long status = 0;
  curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
  if (hs) curl_slist_free_all(hs);
  curl_easy_cleanup(h);
  if (rc != CURLE_OK && !c->aborted)
    luaL_error(L, "http: 傳輸失敗——%s", curl_easy_strerror(rc));
  return (int)status;
}
#endif

/* ══════════════════════════ Lua 進入點 ══════════════════════════ */

static int http_do(lua_State *L, int streaming) {
  luaL_checktype(L, 1, LUA_TTABLE);
  size_t urllen;
  const char *url = opt_str(L, 1, "url", NULL, &urllen);
  if (!url) luaL_error(L, "http: 缺 url");
  const char *method = opt_str(L, 1, "method", "POST", NULL);
  size_t blen = 0;
  const char *body = opt_str(L, 1, "body", NULL, &blen);
  long timeout_ms = 0;
  lua_getfield(L, 1, "timeout"); if (lua_isinteger(L, -1)) timeout_ms = (long)lua_tointeger(L, -1); lua_pop(L, 1);

  Ctx c; memset(&c, 0, sizeof c);
  c.L = L; c.stream = streaming;
  if (streaming) {
    lua_getfield(L, 1, "on_data");
    if (!lua_isfunction(L, -1)) luaL_error(L, "http.stream: 需要 on_data 函式");
    c.cb_idx = lua_gettop(L);   /* on_data 留在堆疊上，整趟不動 */
  }

  int status;
  if (strncmp(url, "file://", 7) == 0) {
    status = do_file(L, url, &c);
  } else {
#ifdef _WIN32
    /* WinHTTP 標頭要一整條 \r\n 串接字串——用純 C Buf 從 opts.headers 陣列現組
       （避開 luaL_Buffer＋堆疊遍歷的平衡陷阱，同 cjson 的理由）。*/
    Buf hbuf; memset(&hbuf, 0, sizeof hbuf);
    lua_getfield(L, 1, "headers");
    if (lua_istable(L, -1)) {
      lua_Integer n = (lua_Integer)lua_rawlen(L, -1);
      for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, -1, i);
        if (lua_isstring(L, -1)) {
          size_t hl; const char *hs = lua_tolstring(L, -1, &hl);
          buf_append(L, &hbuf, hs, hl);
          buf_append(L, &hbuf, "\r\n", 2);
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);                          /* headers table（或 nil） */
    buf_append(L, &hbuf, "\0", 1);          /* NUL 結尾 */
    status = do_http(L, url, method, body, blen, hbuf.b ? hbuf.b : "", timeout_ms, &c);
    free(hbuf.b);
#else
    status = do_http(L, url, method, body, blen, NULL, timeout_ms, &c);
#endif
  }

  if (c.aborted) { free(c.buf.b); return lua_error(L); }  /* on_data 拋的錯，原樣往上丟 */

  lua_pushinteger(L, status);
  if (streaming) return 1;                         /* 串流：內容走 on_data，只回 status */
  lua_pushlstring(L, c.buf.b ? c.buf.b : "", c.buf.len);
  free(c.buf.b);
  return 2;                                        /* 非串流：status, body */
}

static int http_request(lua_State *L) { return http_do(L, 0); }
static int http_stream(lua_State *L)  { return http_do(L, 1); }

static const luaL_Reg http_funcs[] = {
  { "request", http_request },
  { "stream",  http_stream },
  { NULL, NULL }
};

LUAMOD_API int luaopen_http(lua_State *L) {
#ifndef _WIN32
  curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
  luaL_newlib(L, http_funcs);
  lua_pushliteral(L, "0.1");
  lua_setfield(L, -2, "_version");
  return 1;
}
