// cabi_stream.cpp — 串流路徑：http::stream 逐塊 → SSE 拆框 → on_text 逐段、tool_calls 拼完整才 on_tool。

#include "cabi_internal.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>

namespace cabi_impl {
// 具名子 namespace（非匿名）：避開 glaze COMDAT 跨 TU 撞名（理由見 cabi_request.cpp）。
namespace stream_impl {

constexpr glz::opts kLenient{.error_on_unknown_keys = false};

// 收回（串流）：SSE 每筆 data: 的 delta；tool_calls 逐塊拼（index 對齊）。
struct StreamFn {
  std::optional<std::string> name;
  std::optional<std::string> arguments;
};
struct StreamTC {
  std::optional<int> index;
  std::optional<std::string> id;
  std::optional<StreamFn> function;
};
struct StreamDelta {
  std::optional<std::string> content;
  std::optional<std::vector<StreamTC>> tool_calls;
};
struct StreamChoice {
  StreamDelta delta;
};
struct StreamChunk {
  std::vector<StreamChoice> choices;
};
struct Assembled {
  std::string id, name, arguments;
  bool used = false;
};

} // namespace stream_impl
using namespace stream_impl;

llm_status_t do_stream(llm_context_t *ctx, const llm_client_t *c, const std::string &body,
                       const llm_handlers_t *h) {
  http::Request hreq = make_request(c, body);

  std::string line_buf, raw_all;
  std::vector<Assembled> acc;
  bool first = false, was_cancelled = false;

  int status = http::stream(hreq, [&](std::string_view part) -> bool {
    if (!first) {
      first = true;
      set_phase(ctx, LLM_PHASE_STREAM);
    }
    if (cancelled(ctx)) {
      was_cancelled = true;
      return true; // 中止傳輸（curl 乾淨收尾）
    }
    raw_all.append(part);
    line_buf.append(part);
    size_t nl;
    while ((nl = line_buf.find('\n')) != std::string::npos) {
      std::string line = line_buf.substr(0, nl);
      line_buf.erase(0, nl + 1);
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.rfind("data: ", 0) != 0)
        continue;
      std::string data = line.substr(6);
      if (data == "[DONE]")
        return true; // 串流結束
      StreamChunk chunk{};
      if (glz::read<kLenient>(chunk, data))
        continue;
      if (chunk.choices.empty())
        continue;
      const StreamDelta &d = chunk.choices[0].delta;
      if (d.content && !d.content->empty()) {
        if (h && h->on_text &&
            h->on_text(d.content->data(), d.content->size(), h->text_user))
          return true; // 呼叫端喊停（非取消，屬正常早停）
      }
      if (d.tool_calls) { // 逐塊拼：首塊帶 id/name，後續塊補 arguments 片段
        for (const auto &tc : *d.tool_calls) {
          size_t idx = tc.index ? static_cast<size_t>(*tc.index) : acc.size();
          if (idx >= acc.size())
            acc.resize(idx + 1);
          if (tc.id)
            acc[idx].id = *tc.id;
          if (tc.function) {
            if (tc.function->name)
              acc[idx].name = *tc.function->name;
            if (tc.function->arguments)
              acc[idx].arguments += *tc.function->arguments;
          }
          acc[idx].used = true;
        }
      }
    }
    return false; // 這塊處理完，繼續收
  });

  if (was_cancelled) {
    set_phase(ctx, LLM_PHASE_CANCELLED);
    return LLM_CANCELLED;
  }
  guard_backend_error(status, raw_all); // 後端出錯（4xx／error JSON）→ throw → 外層 catch
  if (h && h->on_tool) {                 // tool_calls 一律拼完整才交付
    for (const auto &a : acc) {
      if (!a.used)
        continue;
      llm_tool_call_t call{a.id.c_str(), a.name.c_str(), a.arguments.c_str()};
      if (h->on_tool(&call, h->tool_user))
        break;
    }
  }
  set_phase(ctx, LLM_PHASE_DONE);
  return LLM_OK;
}

} // namespace cabi_impl
