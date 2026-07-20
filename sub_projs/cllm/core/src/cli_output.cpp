// cli_output.cpp — cli_output.hpp 的實作：Sink 的四路出口回呼。

#include "cli_output.hpp"

#include <cstdio>
#include <string_view>

#include <glaze/glaze.hpp> // write_json / raw_json_view（tool_calls 輸出行）
#include "cli_config.hpp"  // save_media

namespace cli::output {
namespace out_impl { // 具名子 namespace（反射型不放匿名 namespace，見 gotchas）

// --tool 的 tool_calls 輸出行（一行一則 JSON）：arguments 原樣內嵌（後端保證是 JSON）。
struct ToolCallLine {
  std::string_view tool, id;
  glz::raw_json_view arguments;
};

} // namespace out_impl

llm::abi::Handlers Sink::handlers() {
  llm::abi::Handlers h;
  h.on_text = [this](std::string_view sv) {
    std::fwrite(sv.data(), 1, sv.size(), stdout); // 串流逐段即時 flush；非串流一次整段
    std::fflush(stdout);
    wrote_text = true;
    return false; // 不中止
  };
  h.on_tool = [](const llm::abi::ToolCall &tc) {
    // ⚠ 全程用 string_view：三元式若混 const char* 與 string 會生暫時 string，raw_json_view 指進去就懸掛
    std::string_view args = tc.arguments.empty() ? std::string_view{"{}"} : std::string_view{tc.arguments};
    out_impl::ToolCallLine line{.tool = tc.name, .id = tc.id, .arguments = glz::raw_json_view{args}};
    if (auto s = glz::write_json(line)) {
      std::fprintf(stdout, "%s\n", s->c_str());
      std::fflush(stdout);
    }
    return false;
  };
  h.on_media = [this](const llm::abi::MediaOut &m) {
    if (media_out_dir.empty()) { // 沒地方放＝明說丟棄
      std::fprintf(stderr, "收到產出媒體（%s，%zu bytes）但沒給 --media-out，已丟棄\n",
                   m.mime.c_str(), m.bytes.size());
      return false;
    }
    std::string path, err;
    if (!config::save_media(media_out_dir, ++media_n, m, path, err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      media_err = true;
      return false;
    }
    std::fprintf(stdout, "%s\n", path.c_str());
    std::fflush(stdout);
    return false;
  };
  h.on_error = [](std::string_view msg) {
    std::fprintf(stderr, "請求失敗：%.*s\n", static_cast<int>(msg.size()), msg.data());
  };
  return h;
}

} // namespace cli::output
