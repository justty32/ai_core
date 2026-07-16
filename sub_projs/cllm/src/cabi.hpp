#pragma once
// cabi.hpp — cllm：C ABI 的「C++ 薄鏡像」總覽傘檔（對應 C ABI 的 cabi.h）。header-only。
//
// 定位：就是 cabi.h 那套 C ABI 的 C++ 版，一比一對應，只捨去 C 的麻煩——
//   · const char*      → std::string
//   · 指標 + count      → std::vector
//   · 函數指標 + void*  → std::function（閉包自帶狀態，void* user 消失）
//   · field_mask + 值   → std::optional（省掉記帳）
// 出口保持 C ABI 的「對稱三 handler」（on_text/on_tool/on_media + on_error），ask() 回 Status。
// ★ 反射糖（ask_as<T>、make_tool<Args>…）刻意不放這層——那是「方便使用」的便利層，
//   已在薄鏡像上包好：bindings/cpp/llm.hpp（零依賴人體工學）＋ llm_reflect.hpp（glaze 糖）。
//   本檔只依賴 cabi.h，不碰 glaze。
//
// 按功能拆（平行於 C ABI 頭），這裡全數 include——客戶端只要 #include "cabi.hpp"：
//   · cabi_request.hpp   輸入型（ToolDef／Schema／MediaIn／Modality／Request）
//   · cabi_response.hpp  收回型＋回呼（ToolCall／MediaOut／Handlers）
//   · cabi_context.hpp   非同步控制（Phase／Context）
// ★ 結構差異：C ABI 扁平，llm_client_t 能獨立成 cabi_client.h；但 C++ 的 Client 把「設定欄位＋
//   ask() 入口」融成一個 class，拆不開——故 Client 連同 Status 放這傘檔（正如 llm_ask 放 cabi.h）。
//
// 命名：放進子 namespace llm::abi（＝C ABI 的 C++ 面）。C ABI 符號是全域 extern "C"（llm_ask…），
//   本層薄薄轉呼叫它們，連結時連 libcllm。

#include "cabi.h" // C ABI（全域 extern "C"：llm_ask / llm_client_t / …）

#include "cabi_context.hpp"
#include "cabi_request.hpp"
#include "cabi_response.hpp"

#include <optional>
#include <string>
#include <vector>

namespace llm::abi {

// ask 的回傳（對應 llm_status_t 的排列：<0 取消、0 成功、>0 錯誤）。
enum class Status {
  Cancelled = -1,
  Ok = 0,
  Error = 1,
};

// 設定好的呼叫端（對應 llm_client_t＋llm_ask；取樣屬性用 optional，未設＝不送、交後端默認）。
class Client {
public:
  std::string endpoint = "http://localhost:1234/v1/chat/completions";
  std::string api_key;
  long timeout_ms = 0;
  std::optional<std::string> model;
  std::optional<float> temperature, top_p, presence_penalty, frequency_penalty;
  std::optional<int> max_tokens, seed;

  // ★ 統一發問(阻塞，對應 llm_ask)。把 Request/Handlers 搬成 C ABI 結構、呼叫 llm_ask、回 Status。
  //   ctx 非 NULL＝可從別的 thread cancel()／phase()。所有輸出走 handlers；本層不聚合、不拋例外。
  //   用法：
  //     Client c;
  //     c.ask({.prompt = "你好"}, {.on_text = [](auto sv){ std::fputs(...); return false; }});
  //     Context ctx; std::thread t([&]{ c.ask({.prompt="很長的活"}, h, &ctx); }); … ctx.cancel();
  Status ask(const Request &req, const Handlers &handlers = {},
             Context *ctx = nullptr) const {
    llm_client_t cc = c_client();

    // 輸入搬運（暫存物件都活到 llm_ask 結束）
    llm_schema_t sc{};
    std::vector<llm_tool_def_t> ct;
    std::vector<llm_media_in_t> cm;
    std::vector<llm_modality_t> co;

    llm_request_t r{};
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
    llm_handlers_t h{};
    if (handlers.on_text) {
      h.on_text = [](const char *t, size_t n, void *u) -> int {
        return (*static_cast<const std::function<bool(std::string_view)> *>(u))(
                   std::string_view(t, n))
                   ? 1
                   : 0;
      };
      h.text_user = const_cast<void *>(static_cast<const void *>(&handlers.on_text));
    }
    if (handlers.on_tool) {
      h.on_tool = [](const llm_tool_call_t *call, void *u) -> int {
        return (*static_cast<const std::function<bool(const ToolCall &)> *>(u))(
                   ToolCall{call->id ? call->id : "", call->name ? call->name : "",
                            call->arguments ? call->arguments : ""})
                   ? 1
                   : 0;
      };
      h.tool_user = const_cast<void *>(static_cast<const void *>(&handlers.on_tool));
    }
    if (handlers.on_media) {
      h.on_media = [](const llm_media_out_t *m, void *u) -> int {
        return (*static_cast<const std::function<bool(const MediaOut &)> *>(u))(
                   MediaOut{m->mime ? m->mime : "",
                            std::string(m->data ? m->data : "", m->data ? m->len : 0)})
                   ? 1
                   : 0;
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

    return static_cast<Status>(llm_ask(ctx ? &ctx->c_ : nullptr, &cc, &r, &h));
  }

private:
  // 把 optional 取樣屬性攤成 C ABI 的 client（const char* 指向 *this 成員，呼叫期間有效）。
  llm_client_t c_client() const {
    llm_client_t c{};
    c.endpoint = endpoint.c_str();
    c.api_key = api_key.c_str();
    c.model = model ? model->c_str() : nullptr;
    c.timeout_ms = timeout_ms;
    c.field_mask = 0;
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
