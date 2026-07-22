# C++ 鏡像參考 — `cabi.hpp` / `llm::abi`

← [docs 索引](README.md)｜[技術入口 README](../README.md)｜[C ABI 參考](c-abi-reference.md)｜[CLI 手冊](cli-manual.md)

`cabi.hpp` 是 C ABI 的 **C++ 薄鏡像**（header-only）——一比一對應 [C ABI](c-abi-reference.md)，只捨去 C 的麻煩：

| C ABI | C++ 鏡像（`llm::abi`）|
|-------|----------------------|
| `const char*` | `std::string` |
| 指標 + count | `std::vector` |
| 函數指標 + `void* user` | `std::function`（閉包自帶狀態，`user` 消失）|
| `field_mask` + 值 | `std::optional`（省掉記帳）|
| `llm_status_t` | `enum class Status` |

```cpp
#include "cabi.hpp"   // header-only；連結時仍連 libcllm（薄薄轉呼叫 llm_ask）
```

> ★ **反射糖刻意不放這層**（`ask<T>`、`make_tool<Args>`、`modality<Config>`）：那是「方便使用」的便利層，`cabi.hpp` 只依賴 `cabi.h`、不碰 glaze，留給使用者自己在薄鏡像上包。`llm` CLI 就是這薄鏡像的第一個消費端。這層便利層現已存在：[`../bindings/cpp/llm.hpp`](../bindings/cpp/llm.hpp)（＋ `llm_reflect.hpp` 的反射糖）。

---

## `Status`（對應 `llm_status_t`）

```cpp
enum class Status {
  Cancelled = -1,
  Ok        =  0,
  Error     =  1,
};
```

---

## `Client`（對應 `llm_client_t` ＋ `llm_ask`）

C ABI 扁平，`llm_client_t` 能獨立成一檔；但 C++ 把「設定欄位 + `ask()` 入口」融成**一個 class**，拆不開。取樣屬性用 `std::optional`——未設 = 不送、交後端默認。

```cpp
class Client {
public:
  std::string endpoint = "http://localhost:1234/v1/chat/completions";
  std::string api_key;
  long        timeout_ms = 0;
  std::optional<std::string> model;
  std::optional<float> temperature, top_p, presence_penalty, frequency_penalty;
  std::optional<int>   max_tokens, seed;

  Status ask(const Request  &req,
             const Handlers &handlers = {},
             Context        *ctx = nullptr) const;
};
```

- `ask()` **阻塞**（對應 `llm_ask`）：把 `Request`/`Handlers` 搬成 C 結構、呼叫 `llm_ask`、回 `Status`。本層**不聚合、不拋例外**，所有輸出走 handlers。
- `ctx` 非 `nullptr` = 可從別的 thread `cancel()`／`phase()`。
- 取樣欄位是 `std::optional`：有值才會設對應的 `field_mask` bit，沒值就不送。

---

## 輸入型（`cabi_request.hpp`）

純 C++ struct，搬進 `llm_ask` 前才在 `Client::ask` 轉成 C 結構。

```cpp
struct ToolDef {
  std::string name, description, parameters;  // parameters = 參數的 JSON Schema 字串
};

struct Schema {
  std::string name = "response";
  std::string json;                           // JSON Schema「物件」字串
};

// (a) 輸入媒體：url（https:// 或 data URI）或 bytes 二選一（都給時 url 優先）；種類看 mime。
struct MediaIn {
  std::string url;
  std::string mime;   // 走 data URI 時可省；走 bytes 時必帶
  std::string bytes;  // 原始位元組（免先 base64；對應 C ABI 的 data/len）
};

// (c) 想要的輸出模態（請求指令，非媒體）：模態名 + 該模態生成參數的 JSON 物件字串。
struct Modality {
  std::string name;    // "text"/"audio"/"image"/"video"
  std::string config;  // 該模態參數 JSON；空 = 默認
};

// 一次發問的輸入（對應 llm_request_t）。schema／tools／media／modalities 可任意組合。
struct Request {
  std::string prompt;
  std::string system;  // 空 = 不送；非空 = 在 user 訊息前插一則 system role 訊息
  std::optional<Schema>   schema;
  std::vector<ToolDef>    tools;
  std::vector<MediaIn>    media;
  std::vector<Modality>   modalities;
  bool stream = false;
};
```

> `stream` 與 `tools` 正交——text/schema/media 皆可串流，`tool_calls` 一律拼完整才交給 `on_tool`。

---

## 輸出型與回呼（`cabi_response.hpp`）

handler 用 `std::function`（閉包自帶狀態，故無 C ABI 的 `void* user`）。

```cpp
struct ToolCall { std::string id, name, arguments; };
struct MediaOut { std::string mime, bytes; };
struct Usage    { int prompt_tokens = -1, completion_tokens = -1, total_tokens = -1; }; // -1 = 後端沒給

// 出口回呼集（對應 llm_handlers_t）。on_text/on_tool/on_media 回 true = 中止；任一可留空（該類輸出被丟棄）。
struct Handlers {
  std::function<bool(std::string_view)>   on_text;   // 文字：串流逐段／非串流整段一次
  std::function<bool(const ToolCall &)>   on_tool;   // 每個工具呼叫（拼完整）
  std::function<bool(const MediaOut &)>   on_media;  // 每則產出媒體
  std::function<void(std::string_view)>   on_error;  // 失敗時一次
  std::function<void(const Usage &)>      on_usage;  // 用量：後端有回才呼叫，至多一次（最後）
};
```

> `on_usage` 裝上＋串流＝請求多送 `stream_options.include_usage`（要後端把 usage 附在末塊）；留空就完全不送。細節見 [C ABI 輸出型](c-abi-output.md#llm_usage_t--token-用量後端回報的-usage)。

> ⚠ **回傳語意與 C ABI 一致但值相反的陷阱**：C ABI 的 handler「回非 0 = 中止」；C++ 這層「回 `true` = 中止」。鏡像內部把 `true`→`1` 轉好，寫 C++ 端照 `true`/`false` 想即可。

---

## 非同步控制（`cabi_context.hpp`）

trivial、內嵌物件內、免 `new`/`free`。用法：另開 thread 跑阻塞的 `ask`，本 thread 持同一 `&ctx` 戳。

```cpp
enum class Phase { Idle, Connect, Upload, Wait, Stream, Done, Error, Cancelled };

class Context {
public:
  Context();
  Context(const Context &) = delete;             // 控制塊不可複製／搬移（別的 thread 持著 &它）
  Context &operator=(const Context &) = delete;
  void  cancel();                                // 任一 thread：請求取消
  Phase phase() const;                           // 任一 thread：原子讀當前階段
};
```

---

## 用法範例

**同步、非串流**：

```cpp
#include "cabi.hpp"

llm::abi::Client c;
c.endpoint = "http://localhost:1234/v1/chat/completions";
c.temperature = 0.7f;                            // optional，未設就不送
c.ask({.prompt = "用一句話介紹你自己"},
      {.on_text = [](std::string_view sv) {
         std::fwrite(sv.data(), 1, sv.size(), stdout);
         return false;                           // 不中止
      }});
```

**串流 + 另一條 thread 取消**：

```cpp
llm::abi::Client c;
llm::abi::Context ctx;
llm::abi::Handlers h{ .on_text = [](std::string_view sv){ /* 逐段 */ return false; } };
std::thread t([&]{ c.ask({.prompt = "很長的活", .stream = true}, h, &ctx); });
// … 需要時：
ctx.cancel();                                    // 串流會在下個安全點乾淨中止
t.join();
```

**結構化輸出**：

```cpp
c.ask({.prompt = "生成一個傲嬌角色",
       .schema = llm::abi::Schema{.json = R"({"type":"object", ...})"}},
      {.on_text = [](std::string_view sv){ /* 收到符合 schema 的 JSON */ return false; }});
```

---

## 相關文檔

- **C 版原始定義（記憶體約定／每欄語意）** → [C ABI 參考](c-abi-reference.md)（＋[輸入型](c-abi-input.md)／[輸出型](c-abi-output.md)）
- **不寫程式碼直接用** → [CLI 手冊](cli-manual.md)
- **視覺總覽** → [`overview.html`](overview.html)
