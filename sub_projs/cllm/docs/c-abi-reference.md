# C ABI 參考 — `libcllm.so` / `cabi.h`（總覽 + 入口）

← [docs 索引](README.md)｜[技術入口 README](../README.md)｜[C++ 鏡像](cpp-mirror-reference.md)｜[CLI 手冊](cli-manual.md)

`libcllm.so` 的**對外 C ABI**——純 C 客戶端 `#include "cabi.h"`、連結 `-lcllm` 即用。整套 ABI 收成**唯一入口 `llm_ask`**：一次吃 prompt＋schema＋tools＋media＋modalities＋stream，輸出全走 handlers。

本檔＝**總覽 + 統一入口**；型別參考拆兩半：
- **[C ABI · 輸入型](c-abi-input.md)** — 呼叫端設定（`llm_client_t`／`field_mask`）＋請求輸入（schema／tool_def／media_in／modality／request）。
- **[C ABI · 輸出型與控制](c-abi-output.md)** — 收回型與四 handler（tool_call／media_out／handlers）＋非同步控制（context／phase／cancel）。

> **穩定介面警語**：扁平結構與 `llm_ask` 簽章是穩定 ABI，改它會震到所有 C 客戶端與 `llm` CLI。實際定義以 `src/cabi*.h` 為準（本檔是它們的導覽視圖）。

## 傘檔佈局（`cabi.h`）

`cabi.h` 是總覽傘檔，`#include` 底下四個功能頭，客戶端只要 include 這一個：

| 頭檔 | 內容 | 參考 |
|------|------|------|
| `cabi_client.h` | 連線 + 取樣設定（`llm_client_t`、`LLM_FIELD_*`）| [輸入型](c-abi-input.md#呼叫端設定cabi_clienth) |
| `cabi_request.h` | 一次發問的「輸入」（schema／tool_def／media_in／modality／request）| [輸入型](c-abi-input.md#請求輸入cabi_requesth) |
| `cabi_response.h` | 一次發問的「收回」（tool_call／media_out／四 handler）| [輸出型](c-abi-output.md#回應輸出cabi_responseh) |
| `cabi_context.h` | 非同步控制（context／phase／cancel）| [輸出型](c-abi-output.md#非同步控制cabi_contexth) |

傘檔自身再補 `llm_status_t` ＋ `llm_ask`（見下）。

## 記憶體約定（整套 ABI 通則）

**本 ABI 不配置任何要呼叫端釋放的東西**——沒有 `llm_free`、沒有配置器。

- 所有 `const char*` 由**呼叫端持有**，只需活到 `llm_ask` 回傳為止。
- 本地媒體檔讀進自己的 buffer，用 `llm_media_in_t` 的 `data`/`len`＋`mime` 直接傳；base64 由 impl 內部處理。
- handler 收到的 `const char*`／指標**只在該次回呼期間有效**，要留就自己複製。

---

## 統一入口 `llm_ask`

```c
typedef enum {
  LLM_CANCELLED = -1,  /* 被取消（phase 也落在 LLM_PHASE_CANCELLED）*/
  LLM_OK        =  0,  /* 成功 */
  LLM_ERROR     =  1   /* 傳輸／後端錯誤（on_error 已被呼叫一次）*/
} llm_status_t;

llm_status_t llm_ask(llm_context_t       *ctx,       /* 可 NULL：不需取消／觀測時 */
                     const llm_client_t  *client,    /* 連線 + 取樣設定 */
                     const llm_request_t *req,       /* 一次發問的輸入 */
                     const llm_handlers_t *handlers);/* 出口回呼（任一可 NULL）*/
```

**阻塞**：在呼叫端這條 thread 跑完整條交換，handler 也在這條 thread 上被呼叫。

| 回傳 | 意義 |
|------|------|
| `LLM_OK` (0) | 成功。輸出已全數經 handler 交付。|
| `LLM_ERROR` (1) | 傳輸失敗／後端回錯。若有 `on_error`，已被呼叫一次帶訊息。|
| `LLM_CANCELLED` (-1) | 經 `ctx` 被別的 thread 取消。|

> 回傳值編碼刻意讓「被取消」(<0) 與「出錯」(>0) 分邊，成功是 0。

**最小範例**：

```c
#include "cabi.h"

static int on_text(const char *t, size_t n, void *user) {
  fwrite(t, 1, n, stdout);   /* 串流逐段／非串流整段 */
  return 0;                  /* 回非 0 = 要求中止 */
}

int main(void) {
  llm_client_t   c = { .endpoint = "http://localhost:1234/v1/chat/completions" };
  llm_request_t  r = { .prompt = "用一句話介紹你自己", .stream = 1 };
  llm_handlers_t h = { .on_text = on_text };
  return llm_ask(NULL, &c, &r, &h) == LLM_OK ? 0 : 1;
}
```

編譯：`cc demo.c -lcllm -o demo`（頭檔在 `src/`，庫在 `build/libcllm.so`）。

**四塊正交欄位**（同一個請求 body，愛怎麼組就怎麼組——這正是「一個入口」而非四個函數的理由）：

```
                         ┌──────────────┐
   prompt ──────────────►│              │──► on_text   （文字：串流逐段／整段）
   schema（結構化）──────►│   llm_ask    │──► on_tool   （工具呼叫：拼完整才交）
   tools（工具定義）─────►│  組成一個     │──► on_media  （產出媒體，如 audio）
   media（輸入圖／音）───►│  OpenAI 請求  │──► on_error  （傳輸／後端錯誤）
   modalities（輸出模態）►│              │
   stream ──────────────►└──────────────┘   ctx（可選）：另 thread cancel()/phase()
```

---

## 相關文檔

- **輸入型（client／request）** → [c-abi-input.md](c-abi-input.md)
- **輸出型與控制（handlers／context）** → [c-abi-output.md](c-abi-output.md)
- **上手 / 建置 / 接真後端 / 跨平台 / 除錯** → [README.md](../README.md)
- **C++ 版（`llm::abi`）** → [C++ 鏡像參考](cpp-mirror-reference.md)
- **`llm` CLI（不寫一行 C 就能用）** → [CLI 手冊](cli-manual.md)
- **視覺總覽（一眼看懂資料流）** → [`overview.html`](overview.html)
- **踩坑（glaze／匿名 namespace COMDAT／後端錯誤護欄）** → [common/gotchas](../workflows/common/gotchas/README.md)
