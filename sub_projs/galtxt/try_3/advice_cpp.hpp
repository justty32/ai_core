#pragma once
// advice_cpp.hpp — galtxt try_3：[L2] C ABI 的「C++ 薄鏡像」（設計提案），包在 C ABI 外。
//
// 定位：就是 llm_cabi 的 C++ 版，一比一對應，只捨去 C 的麻煩——
//   · const char*      → std::string
//   · 指標 + count      → std::vector
//   · 函數指標 + void*  → std::function（閉包自帶狀態，void* user 消失）
//   · field_mask + 值   → std::optional（省掉記帳）
// 出口保持 C ABI 的「對稱三 handler」（on_text/on_tool/on_media + on_error），ask() 回 Status。
// ★ 反射糖（ask<T>、make_tool<Args>、modality<Config>）刻意不放這層——那是「方便使用」的便利層，
//   之後由使用者自己在這薄鏡像上包裝。故本檔只依賴 advice.hpp，不碰 glaze / llm_schema.hpp。
//
// 命名：放進子 namespace llm::abi（＝C ABI 的 C++ 面），與 L0 的 llm::Client 徹底分流——
//    llm::（L0 核心）／llm_cabi::（C ABI）／llm::abi::（本層 C++ 鏡像），三層各據其名、可同 TU 共存。

#include "advice.hpp" // L1 C ABI（llm_cabi::*）

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace llm::abi {

// ── 型別：C ABI 各 struct 的 C++ 版（const char* → string；指標+count 由持有它的 vector 管）──
struct ToolDef {
  std::string name, description, parameters;
}; // parameters＝參數的 JSON Schema 字串（自備；反射生成留給上層便利層）
struct ToolCall {
  std::string id, name, arguments;
};
struct Schema {
  std::string name = "response";
  std::string json; // JSON Schema「物件」字串
};

// (a) 輸入媒體：url（https:// 或 data URI）或 bytes 二選一（都給時 url 優先）；種類看 mime。
struct MediaIn {
  std::string url;
  std::string mime;  // 走 data URI 時可省；走 bytes 時必帶
  std::string bytes; // 原始位元組（免先 base64；對應 C ABI 的 data/len）
};
// (b) 產出媒體：模型產物（如生成的 audio），bytes＝原始位元組，種類看 mime。
struct MediaOut {
  std::string mime;
  std::string bytes;
};
// (c) 想要的輸出模態（請求指令，非媒體）：模態名 + 該模態生成參數的 JSON 物件字串。
struct Modality {
  std::string name;   // "text"/"audio"/"image"/"video"
  std::string config; // 該模態參數 JSON；空＝默認
};

// 一次發問的輸入（對應 llm_request_t）。schema／tools／media／modalities 可任意組合；
// stream 與 tools 正交——text/schema/media 皆可串流，tool_calls 一律拼完整才交給 on_tool。
struct Request {
  std::string prompt;
  std::optional<Schema> schema;
  std::vector<ToolDef> tools;
  std::vector<MediaIn> media;
  std::vector<Modality> modalities;
  bool stream = false;
};

// 出口回呼集（對應 llm_handlers_t）。用 std::function：閉包自帶狀態，故無 void* user。
// on_text/on_tool/on_media 回 true＝中止（對應 C ABI 回非 0）；任一可留空（＝該類輸出被丟棄）。
struct Handlers {
  std::function<bool(std::string_view)> on_text;   // 文字：串流逐段／非串流整段一次
  std::function<bool(const ToolCall &)> on_tool;   // 每個工具呼叫（拼完整）
  std::function<bool(const MediaOut &)> on_media;  // 每則產出媒體
  std::function<void(std::string_view)> on_error;  // 失敗時一次
};

// ── 非同步控制（可選）：跨 thread 的取消 + 階段觀測（對應 C ABI 的 llm_context_t）──
// trivial、內嵌物件內、免 new/free。用法：另開 thread 跑阻塞的 ask，本 thread 持同一 &ctx 戳。
enum class Phase {
  Idle, Connect, Upload, Wait, Stream, Done, Error, Cancelled
};
class Context {
public:
  Context() : c_{0, 0} {}
  Context(const Context &) = delete;            // 控制塊不可複製/搬移（別的 thread 持著 &它）
  Context &operator=(const Context &) = delete;
  void cancel() { llm_cabi::llm_cancel(&c_); }  // 任一 thread：請求取消
  Phase phase() const {                          // 任一 thread：原子讀當前階段
    return static_cast<Phase>(llm_cabi::llm_phase(&c_));
  }

private:
  friend class Client;
  llm_cabi::llm_context_t c_; // 就在物件裡，隨物件生滅、無 heap
};

// ask 的回傳（對應 llm_status_t 的排列：<0 取消、0 成功、>0 錯誤）。
enum class Status {
  Cancelled = -1,
  Ok        = 0,
  Error     = 1,
};

// 設定好的呼叫端（對應 llm_client_t；取樣屬性用 optional，未設＝不送、交後端默認）。
class Client {
public:
  std::string endpoint = "http://localhost:1234/v1/chat/completions";
  std::string api_key;
  long timeout_ms = 0;
  std::optional<std::string> model;
  std::optional<float> temperature, top_p, presence_penalty, frequency_penalty;
  std::optional<int> max_tokens, seed;

  // ★ 統一發問（阻塞，對應 llm_ask）。把 Request/Handlers 搬成 C ABI 結構、呼叫 llm_ask、回 Status。
  //   ctx 非 NULL＝可從別的 thread cancel()／phase()。所有輸出走 handlers；本層不聚合、不拋例外。
  //   用法：
  //     Client c;
  //     c.ask({.prompt = "你好"}, {.on_text = [](auto sv){ std::fputs(...); return false; }});
  //     Context ctx; std::thread t([&]{ c.ask({.prompt="很長的活"}, h, &ctx); }); … ctx.cancel();
  Status ask(const Request &req, const Handlers &handlers = {},
             Context *ctx = nullptr) const {
    llm_cabi::llm_client_t cc = c_client();

    // 輸入搬運（暫存物件都活到 llm_ask 結束）
    llm_cabi::llm_schema_t sc{};
    std::vector<llm_cabi::llm_tool_def_t> ct;
    std::vector<llm_cabi::llm_media_in_t> cm;
    std::vector<llm_cabi::llm_modality_t> co;

    llm_cabi::llm_request_t r{};
    r.prompt = req.prompt.c_str();
    r.stream = req.stream ? 1 : 0;
    if (req.schema) {
      sc.name = req.schema->name.c_str();
      sc.json = req.schema->json.c_str();
      r.schema = &sc;
    }
    if (!req.tools.empty()) {
      for (const auto &t : req.tools)
        ct.push_back({t.name.c_str(), t.description.c_str(), t.parameters.c_str()});
      r.tools = ct.data();
      r.tools_count = ct.size();
    }
    if (!req.media.empty()) {
      for (const auto &m : req.media)
        cm.push_back({m.url.empty() ? nullptr : m.url.c_str(),
                      m.mime.empty() ? nullptr : m.mime.c_str(),
                      m.bytes.empty() ? nullptr : m.bytes.data(), m.bytes.size()});
      r.media = cm.data();
      r.media_count = cm.size();
    }
    if (!req.modalities.empty()) {
      for (const auto &o : req.modalities)
        co.push_back({o.name.c_str(), o.config.empty() ? nullptr : o.config.c_str()});
      r.modalities = co.data();
      r.modalities_count = co.size();
    }

    // 出口：C ABI 的 handler（非捕獲 lambda → C 函數指標）蹦床到 std::function，
    //       user 指標指向對應的 std::function（handlers 是 const 參考，活到 llm_ask 結束）。
    llm_cabi::llm_handlers_t h{};
    if (handlers.on_text) {
      h.on_text = [](const char *t, size_t n, void *u) -> int {
        return (*static_cast<const std::function<bool(std::string_view)> *>(u))(
                   std::string_view(t, n)) ? 1 : 0;
      };
      h.text_user = const_cast<void *>(static_cast<const void *>(&handlers.on_text));
    }
    if (handlers.on_tool) {
      h.on_tool = [](const llm_cabi::llm_tool_call_t *call, void *u) -> int {
        return (*static_cast<const std::function<bool(const ToolCall &)> *>(u))(
                   ToolCall{call->id ? call->id : "",
                            call->name ? call->name : "",
                            call->arguments ? call->arguments : ""}) ? 1 : 0;
      };
      h.tool_user = const_cast<void *>(static_cast<const void *>(&handlers.on_tool));
    }
    if (handlers.on_media) {
      h.on_media = [](const llm_cabi::llm_media_out_t *m, void *u) -> int {
        return (*static_cast<const std::function<bool(const MediaOut &)> *>(u))(
                   MediaOut{m->mime ? m->mime : "",
                            std::string(m->data ? m->data : "", m->data ? m->len : 0)})
                   ? 1 : 0;
      };
      h.media_user = const_cast<void *>(static_cast<const void *>(&handlers.on_media));
    }
    if (handlers.on_error) {
      h.on_error = [](const char *msg, size_t n, void *u) {
        (*static_cast<const std::function<void(std::string_view)> *>(u))(
            std::string_view(msg, n));
      };
      h.error_user = const_cast<void *>(static_cast<const void *>(&handlers.on_error));
    }

    return static_cast<Status>(
        llm_cabi::llm_ask(ctx ? &ctx->c_ : nullptr, &cc, &r, &h));
  }

private:
  // 把 optional 取樣屬性攤成 C ABI 的 client（const char* 指向 *this 成員，呼叫期間有效）。
  llm_cabi::llm_client_t c_client() const {
    llm_cabi::llm_client_t c{};
    c.endpoint = endpoint.c_str();
    c.api_key = api_key.c_str();
    c.model = model ? model->c_str() : nullptr;
    c.timeout_ms = timeout_ms;
    c.field_mask = 0;
    using namespace llm_cabi;
    if (temperature) {
      c.temperature = *temperature;
      llm_client_set_field(&c, LLM_FIELD_TEMPERATURE, 1);
    }
    if (top_p) {
      c.top_p = *top_p;
      llm_client_set_field(&c, LLM_FIELD_TOP_P, 1);
    }
    if (presence_penalty) {
      c.presence_penalty = *presence_penalty;
      llm_client_set_field(&c, LLM_FIELD_PRESENCE_PENALTY, 1);
    }
    if (frequency_penalty) {
      c.frequency_penalty = *frequency_penalty;
      llm_client_set_field(&c, LLM_FIELD_FREQUENCY_PENALTY, 1);
    }
    if (max_tokens) {
      c.max_tokens = *max_tokens;
      llm_client_set_field(&c, LLM_FIELD_MAX_TOKENS, 1);
    }
    if (seed) {
      c.seed = *seed;
      llm_client_set_field(&c, LLM_FIELD_SEED, 1);
    }
    return c;
  }
};

} // namespace llm::abi
