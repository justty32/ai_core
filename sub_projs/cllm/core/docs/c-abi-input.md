# C ABI · 輸入型 — 呼叫端設定 + 請求輸入

← [C ABI 總覽](c-abi-reference.md)｜[輸出型與控制](c-abi-output.md)｜[docs 索引](README.md)

`llm_ask` 吃的兩塊：**`llm_client_t`**（連線 + 取樣，`cabi_client.h`）與 **`llm_request_t`**（一次發問的輸入，`cabi_request.h`）。

---

## 呼叫端設定（`cabi_client.h`）

### `llm_client_t`

連線 + 取樣。指標欄 `NULL` = 未設；6 個**數值取樣欄位**是否生效看 `field_mask`。

```c
typedef struct llm_client_t {
  const char *endpoint;     /* NULL = 內建預設 http://localhost:1234/v1/chat/completions */
  const char *api_key;      /* NULL/"" = 不送 Authorization: Bearer */
  const char *model;        /* NULL = 不送 model（本地後端 = 用已載入模型）*/
  long        timeout_ms;   /* 0 = 不設逾時 */
  unsigned    field_mask;   /* 哪些數值取樣欄位有設（LLM_FIELD_* 的 OR）*/
  float temperature, top_p, presence_penalty, frequency_penalty;
  int   max_tokens, seed;
} llm_client_t;
```

| 欄位 | 型別 | 未設語意 | 備註 |
|------|------|----------|------|
| `endpoint` | `const char*` | 內建 `http://localhost:1234/v1/chat/completions` | 到 `/chat/completions` 為止的完整 URL；`file://<絕對路徑>` 走離線 fixture |
| `api_key` | `const char*` | 不送 `Authorization` | 雲端 API 必給 |
| `model` | `const char*` | 不送 `model` 欄位 | ⚠ 本地伺服器可省，**OpenAI 雲端必填** |
| `timeout_ms` | `long` | `0` = 不設逾時 | 真後端（reasoning 模型）建議設 `120000` |
| `temperature` 等 6 欄 | `float`/`int` | 由 `field_mask` 決定，未標的不寫進 JSON | 見下 |

### 取樣欄位的 `field_mask`（位旗標）

C 沒有 `std::optional`，用**位元記帳**表達「這個數值欄位有沒有設」：沒設的欄位不寫進請求 JSON，交後端默認。

```c
enum {
  LLM_FIELD_TEMPERATURE       = 1u << 0,
  LLM_FIELD_TOP_P             = 1u << 1,
  LLM_FIELD_PRESENCE_PENALTY  = 1u << 2,
  LLM_FIELD_FREQUENCY_PENALTY = 1u << 3,
  LLM_FIELD_MAX_TOKENS        = 1u << 4,
  LLM_FIELD_SEED              = 1u << 5
};
```

存取器（`static inline`，也可直接 OR 進 `field_mask`）：

```c
static inline void llm_client_set_field(llm_client_t *c, unsigned field, int on);
static inline int  llm_client_has_field(const llm_client_t *c, unsigned field);
```

```c
llm_client_t c = { .endpoint = "…" };
c.temperature = 0.7f;
llm_client_set_field(&c, LLM_FIELD_TEMPERATURE, 1);   /* 或 c.field_mask |= LLM_FIELD_TEMPERATURE; */
```

> ⚠ **reasoning 模型別亂設 `max_tokens`**：思考鏈與答案共用同一份 token 預算，設太小會讓 reasoning 吃光預算、`content` 變空字串。打 reasoning 模型時建議乾脆不設。詳見 [README「接真後端」](../README.md#接真後端本機-lm-studio)。

---

## 請求輸入（`cabi_request.h`）

### `llm_request_t`

一次發問的輸入。`schema`／`tools`／`media`／`modalities` 可**任意組合**（正交欄位）。

```c
typedef struct llm_request_t {
  const char *prompt;                 /* 必填 */
  const char *system;                 /* NULL/"" = 不送；非空 = 在 user 訊息前插一則 system role 訊息 */
  const llm_schema_t   *schema;       /* NULL = 不用結構化輸出 */
  const llm_tool_def_t *tools;
  size_t                tools_count;
  const llm_media_in_t *media;        /* 輸入媒體（圖／音…）*/
  size_t                media_count;
  const llm_modality_t *modalities;   /* 想要的輸出模態＋參數；NULL/0 = 只文字 */
  size_t                modalities_count;
  int                   stream;       /* 非 0 = 串流 */
} llm_request_t;
```

> **`stream` 與 `tools` 正交**：text/schema/media 皆可串流；`tool_calls` **一律拼完整才交給 `on_tool`**，不受 `stream` 影響。

### `llm_schema_t` — 結構化輸出

```c
typedef struct llm_schema_t {
  const char *name;  /* NULL = "response" */
  const char *json;  /* JSON Schema「物件」字串 */
} llm_schema_t;
```

> ⚠ **`required` 坑**：JSON Schema 下沒列進 `required` 的欄位就是選填，後端不保證會給。實測本地模型對沒標 `required` 的欄位常常漏吐。`json` 是**呼叫端自備的原始 schema 字串**——`required` 要不要標、標哪些，是寫 schema 那方的責任。

### `llm_tool_def_t` — 工具定義（你送給模型看的）

```c
typedef struct llm_tool_def_t {
  const char *name;
  const char *description;
  const char *parameters;  /* 參數的 JSON Schema（物件字串）*/
} llm_tool_def_t;
```

### `llm_media_in_t` — 輸入媒體（圖／音給模型看）

`url` 或 `data`/`len` 二選一（都給時 `url` 優先）；種類一律看 `mime`。

```c
typedef struct llm_media_in_t {
  const char *url;   /* "https://…"（後端自己抓）或 "data:<mime>;base64,…" */
  const char *mime;  /* 走 data URI 時已內含可 NULL；走 data 時必帶 */
  const char *data;  /* 或直接給原始位元組（免自己 base64；impl 內部轉 data URI）*/
  size_t      len;   /* data 的位元組長度（走 url 時為 0）*/
} llm_media_in_t;
```

- 音訊 → 組 `input_audio`＋base64；其餘 → `image_url`（url 或本地 bytes 轉 data URI）。

### `llm_modality_t` — 想要的輸出模態（指令，非媒體）

要模型產哪些模態、各帶什麼生成參數。

```c
typedef struct llm_modality_t {
  const char *name;    /* "text"/"audio"/"image"/"video" */
  const char *config;  /* 該模態生成參數的 JSON 物件字串（NULL/"" = 默認）*/
} llm_modality_t;
```

例：`audio` → `{"voice":"alloy","format":"wav"}`；`image` → `{"size":"1024x1024"}`。

---

**收回那半邊**（模型吐回來的 tool_call／media_out ＋ 四 handler）與**非同步控制** → [C ABI · 輸出型與控制](c-abi-output.md)。
