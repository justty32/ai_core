// cli_output.cpp — cli_output.hpp 的實作：Sink 的四路出口（對齊 core/src/cli_output.cpp）。

#include "cli_output.hpp"

#include <cstdio>

#include <glaze/glaze.hpp> // write_json / raw_json_view（tool_calls 輸出行）

#include "media.hpp" // ext_of

namespace cli::output {
namespace out_impl { // 具名子 namespace（反射型不放匿名 namespace，見 gotchas）
// tool_calls 輸出行（一行一則 JSON）：arguments 原樣內嵌（後端保證是 JSON）。
struct ToolCallLine {
  std::string_view tool, id;
  glz::raw_json_view arguments;
};
} // namespace out_impl

bool Sink::on_delta(std::string_view sv) {
  std::fwrite(sv.data(), 1, sv.size(), stdout); // 串流逐段即時 flush；非串流一次整段
  std::fflush(stdout);
  wrote_text = true;
  return false; // 不中止
}

void Sink::emit_tool_call(const llm::abi::ToolCall &tc) {
  // ⚠ 全程用 string_view：三元式若混 const char* 與 string 會生暫時 string，raw_json_view 指進去就懸掛
  std::string_view args = tc.arguments.empty() ? std::string_view{"{}"} : std::string_view{tc.arguments};
  out_impl::ToolCallLine line{.tool = tc.name, .id = tc.id, .arguments = glz::raw_json_view{args}};
  if (auto s = glz::write_json(line)) {
    std::fprintf(stdout, "%s\n", s->c_str());
    std::fflush(stdout);
  }
}

void Sink::save_media(const llm::abi::MediaOut &m) {
  if (media_out_dir.empty()) { // 沒地方放＝明說丟棄
    std::fprintf(stderr, "收到產出媒體（%s，%zu bytes）但沒給 --media-out，已丟棄\n",
                 m.mime.c_str(), m.bytes.size());
    return;
  }
  std::string path = media_out_dir + "/llm-media-" + std::to_string(++media_n) + "." +
                     cli::media::ext_of(m.mime);
  std::FILE *f = std::fopen(path.c_str(), "wb");
  if (!f || std::fwrite(m.bytes.data(), 1, m.bytes.size(), f) != m.bytes.size()) {
    if (f) std::fclose(f);
    std::fprintf(stderr, "媒體落檔失敗：%s\n", path.c_str());
    media_err = true;
    return;
  }
  std::fclose(f);
  std::fprintf(stdout, "%s\n", path.c_str());
  std::fflush(stdout);
}

} // namespace cli::output
