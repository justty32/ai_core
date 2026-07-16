# C ABI · 輸出型與控制 — handlers + 非同步控制

← [C ABI 總覽](c-abi-reference.md)｜[輸入型](c-abi-input.md)｜[docs 索引](README.md)

`llm_ask` 的**收回面**：模型吐回來的東西全走 **`llm_handlers_t`** 四個回呼（`cabi_response.h`）；跨 thread 的取消／觀測走 **`llm_context_t`**（`cabi_context.h`）。

---

## 回應輸出（`cabi_response.h`）

### `llm_handlers_t` — 出口回呼集

四個 handler，任一可為 `NULL`（該類輸出被丟棄）。每個帶 `void* user` 攜帶狀態。

```c
typedef struct llm_handlers_t {
  llm_text_handler  on_text;   void *text_user;
  llm_tool_handler  on_tool;   void *tool_user;
  llm_media_handler on_media;  void *media_user;  /* 模型產出的媒體（如 audio）*/
  llm_error_handler on_error;  void *error_user;
} llm_handlers_t;
```

### handler 簽章

```c
/* 文字：串流逐段／非串流整段一次。text 非保證 NUL 結尾，長度看 len。回非 0 = 中止。*/
typedef int  (*llm_text_handler )(const char *text, size_t len, void *user);
/* 工具呼叫：模型每要求一個工具呼一次（永遠拼完整才交付）。回非 0 = 中止。*/
typedef int  (*llm_tool_handler )(const llm_tool_call_t *call, void *user);
/* 媒體輸出：模型每產出一則媒體呼一次；media->data/len = 原始位元組。回非 0 = 中止。*/
typedef int  (*llm_media_handler)(const llm_media_out_t *media, void *user);
/* 錯誤：傳輸失敗／後端回錯時呼叫一次。message 只在回呼期間有效。*/
typedef void (*llm_error_handler)(const char *message, size_t len, void *user);
```

> **回非 0 = 要求中止串流**（前三個 handler）。`on_error` 無回傳。
> `text`/`message` 的字串**不保證 NUL 結尾**，一律用 `len`。

### `llm_tool_call_t` — 工具呼叫（模型回來要你執行的）

```c
typedef struct llm_tool_call_t {
  const char *id;
  const char *name;
  const char *arguments;  /* 模型產生的 arguments（JSON 字串）*/
} llm_tool_call_t;
```

### `llm_media_out_t` — 產出媒體（模型生成的 audio…）

純位元組，無 url，種類看 `mime`。

```c
typedef struct llm_media_out_t {
  const char *mime;
  const char *data;  /* 原始位元組 */
  size_t      len;
} llm_media_out_t;
```

---

## 非同步控制（`cabi_context.h`）

`llm_ask` 本身阻塞。要非同步就**由呼叫端自己開 thread 跑 `llm_ask`**，另一條 thread 拿同一個 `ctx` 取消／觀測。

### `llm_context_t`

trivial（兩個 int），呼叫端自行配置（堆疊／靜態／內嵌皆可），`{0}` 或 memset 歸零即用，library 不配置任何東西。

```c
typedef struct llm_context_t {
  int cancel;  /* 0 = 進行中；非 0 = 已請求取消 */
  int phase;   /* 目前階段，值域 = llm_phase_t */
} llm_context_t;
```

> ⚠ **別直接碰欄位**——一律透過 `llm_cancel()`／`llm_phase()` 存取（正確的原子存取封在那兩個函數裡，靠 `std::atomic_ref`）。

### 生命週期階段 `llm_phase_t`

```c
typedef enum {
  LLM_PHASE_IDLE,       /* 尚未開始 */
  LLM_PHASE_CONNECT,    /* 連線中（本版未細分，保留給未來自持 curl）*/
  LLM_PHASE_UPLOAD,     /* 上傳請求中（同上，未細分）*/
  LLM_PHASE_WAIT,       /* 送完、等模型 */
  LLM_PHASE_STREAM,     /* 接收回應中 */
  LLM_PHASE_DONE,       /* 正常完成 */
  LLM_PHASE_ERROR,      /* 失敗 */
  LLM_PHASE_CANCELLED   /* 被取消 */
} llm_phase_t;
```

典型走向：`IDLE → WAIT → STREAM → DONE`（或 `ERROR`／`CANCELLED`）。`CONNECT`／`UPLOAD` 本版走 `http` 這層時未細分，保留給未來自持 curl。

### 函數

```c
void        llm_cancel(llm_context_t *ctx);        /* 任一 thread：請求取消，下個安全點乾淨收連線 */
llm_phase_t llm_phase (const llm_context_t *ctx);  /* 任一 thread：原子讀當前階段 */
```

> ⚠ **取消的邊界**：非串流的底層 `http::request` 是阻塞的，`cancel` 只在派送**前**檢查得到；**串流路徑每塊都檢查、能乾淨中止**。

### 非同步範例

```c
#include "cabi.h"
#include <pthread.h>

struct job { llm_context_t ctx; llm_client_t *c; llm_request_t *r; llm_handlers_t *h; };

static void *worker(void *p) {
  struct job *j = p;
  llm_ask(&j->ctx, j->c, j->r, j->h);   /* 阻塞在這條 thread */
  return NULL;
}

/* 主 thread：起 worker，需要時 llm_cancel(&j.ctx)；輪詢 llm_phase(&j.ctx) 觀測進度。*/
```

---

**送出那半邊**（client 設定 + 請求輸入）→ [C ABI · 輸入型](c-abi-input.md)。**入口 `llm_ask` 與整體** → [C ABI 總覽](c-abi-reference.md)。
