// cabi_response.cpp — 解析非串流回應＋後端錯誤護欄（唯一真相源的「收回」半邊）。

#include "cabi_internal.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <glaze/glaze.hpp>

namespace cabi_impl {
// 具名子 namespace（非匿名）：避開 glaze COMDAT 跨 TU 撞名（理由見 cabi_request.cpp）。
namespace resp_impl {

constexpr glz::opts kLenient{.error_on_unknown_keys = false}; // 後端回應欄位多，寬鬆解析

// 收回（非串流）：只挑要的欄位，其餘靠 kLenient 忽略。
struct RespFn {
  std::string name;
  std::string arguments;
};
struct RespToolCall {
  std::string id;
  RespFn function;
};
struct RespAudio {
  std::optional<std::string> data; // base64
  std::optional<std::string> format;
};
struct RespMsg {
  std::optional<std::string> content; // 工具情形 content=null → nullopt
  std::optional<std::vector<RespToolCall>> tool_calls;
  std::optional<RespAudio> audio;
};
struct RespChoice {
  RespMsg message;
};
struct RespBody {
  std::vector<RespChoice> choices;
};

// 後端錯誤外殼（OpenAI 風格 {"error":{"message":…}}）
struct ErrDetail {
  std::string message;
};
struct ErrEnvelope {
  std::optional<ErrDetail> error;
};

// 小 base64 解碼（媒體輸出用；glaze 只保證有 write_base64，這邊自備 read 免依賴不確定 API）。
std::string base64_decode(std::string_view in) {
  auto val = [](unsigned char c) -> int {
    if (c >= 'A' && c <= 'Z')
      return c - 'A';
    if (c >= 'a' && c <= 'z')
      return c - 'a' + 26;
    if (c >= '0' && c <= '9')
      return c - '0' + 52;
    if (c == '+')
      return 62;
    if (c == '/')
      return 63;
    return -1;
  };
  std::string out;
  int buf = 0, bits = 0;
  for (unsigned char c : in) {
    if (c == '=')
      break;
    int v = val(c);
    if (v < 0)
      continue; // 跳過換行/空白
    buf = (buf << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<char>((buf >> bits) & 0xFF));
    }
  }
  return out;
}

} // namespace resp_impl
using namespace resp_impl;

// 護欄：後端錯誤不得被靜默吞成空字串（原 llm.cpp 的 throw_if_backend_error）。
// 離線 file:// fixture 回 status=200 且無 error 物件 → 不受影響。
void guard_backend_error(int status, const std::string &raw) {
  ErrEnvelope env{};
  if (!glz::read<kLenient>(env, raw) && env.error)
    throw std::runtime_error("後端錯誤 (HTTP " + std::to_string(status) +
                             "): " + env.error->message);
  if (status >= 400)
    throw std::runtime_error("後端錯誤 (HTTP " + std::to_string(status) +
                             "): " + raw.substr(0, 300));
}

// 非串流：解析 choices[0].message，餵 on_text／on_tool／on_media。
void dispatch_nonstream(const std::string &raw, const llm_handlers_t *h) {
  RespBody parsed{};
  if (glz::read<kLenient>(parsed, raw) || parsed.choices.empty())
    return; // 解不動／無 choices（後端錯誤已由 guard 攔掉）
  const RespMsg &m = parsed.choices[0].message;
  if (m.content && !m.content->empty() && h && h->on_text)
    h->on_text(m.content->data(), m.content->size(), h->text_user);
  if (m.tool_calls && h && h->on_tool) {
    for (const auto &tc : *m.tool_calls) {
      llm_tool_call_t call{tc.id.c_str(), tc.function.name.c_str(),
                           tc.function.arguments.c_str()};
      if (h->on_tool(&call, h->tool_user))
        break; // 呼叫端喊停
    }
  }
  if (m.audio && m.audio->data && h && h->on_media) {
    std::string bytes = base64_decode(*m.audio->data);
    std::string mime = "audio/" + (m.audio->format ? *m.audio->format : std::string("wav"));
    llm_media_out_t out{mime.c_str(), bytes.data(), bytes.size()};
    h->on_media(&out, h->media_user);
  }
}

} // namespace cabi_impl
