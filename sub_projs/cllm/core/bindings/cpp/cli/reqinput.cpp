// reqinput.cpp — reqinput.hpp 的實作：請求類旗標 → abi::Request 四件組（對齊 reqinput.py）。

#include "reqinput.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

#include <glaze/glaze.hpp> // json_t / raw_json（字面 JSON 驗證／工具定義解析）

#include "cli_internal.hpp" // kExitOk / kExitUsage
#include "media.hpp"        // build_media_item

namespace cli::reqinput {
namespace tool_impl { // 具名子 namespace（反射型不放匿名 namespace，見 gotchas）
// 字面 --tool 的形狀：parameters 用 raw_json 原樣收（它是「參數的 JSON Schema」，不重組）。
struct ToolDefFile {
  std::string name, description;
  glz::raw_json parameters;
};
inline constexpr glz::opts kLenient{.error_on_unknown_keys = false};
} // namespace tool_impl

// 驗字面文字是合法 JSON（不落地成物件，只確認可解析）。
static bool valid_json(const std::string &text) {
  glz::generic j;
  return !glz::read_json(j, text);
}

int build_request_inputs(const argv::ParsedArgs &p, RequestInputs &out) {
  if (p.has_schema) { // 字面 JSON Schema：只驗合法，原樣交內核嵌入
    if (!valid_json(p.schema_text)) {
      std::fprintf(stderr, "--schema 不是合法 JSON（收字面文字非路徑；長內容用 $(cat s.json)）\n");
      return kExitUsage;
    }
    out.schema = llm::abi::Schema{.name = "response", .json = p.schema_text};
  }

  for (const auto &spec : p.tool_specs) { // 字面 tool def JSON；需 name 與非空 parameters
    tool_impl::ToolDefFile td;
    if (glz::read<tool_impl::kLenient>(td, spec)) {
      std::fprintf(stderr, "--tool 不是合法 JSON（收字面文字非路徑；長內容用 $(cat t.json)）\n");
      return kExitUsage;
    }
    if (td.name.empty() || td.parameters.str.empty()) {
      std::fprintf(stderr, "--tool 缺 name 或 parameters\n");
      return kExitUsage;
    }
    out.tools.push_back(llm::abi::ToolDef{.name = std::move(td.name),
                                          .description = std::move(td.description),
                                          .parameters = std::move(td.parameters.str)});
  }

  for (const auto &spec : p.modality_specs) { // 「名」或「名=<字面JSON>」
    auto eq = spec.find('=');
    std::string name = spec.substr(0, eq); // npos＝整串當名
    if (name.empty()) {
      std::fprintf(stderr, "--modality 缺模態名（如 audio 或 audio={\"voice\":\"alloy\"}）\n");
      return kExitUsage;
    }
    llm::abi::Modality mod{.name = std::move(name)};
    if (eq != std::string::npos) { // 有 '='：cfg 收字面 JSON，驗合法
      std::string cfg = spec.substr(eq + 1);
      if (!valid_json(cfg)) {
        std::fprintf(stderr,
                     "--modality 的 config 不是合法 JSON（收字面文字；長內容用 $(cat cfg.json)）\n");
        return kExitUsage;
      }
      mod.config = std::move(cfg);
    }
    out.modalities.push_back(std::move(mod));
  }

  for (const auto &spec : p.media_specs) { // 三分流（media.hpp）
    auto item = media::build_media_item(spec);
    if (!item)
      return kExitUsage;
    out.media.push_back(std::move(*item));
  }

  if (!p.media_out_dir.empty() && !std::filesystem::is_directory(p.media_out_dir)) {
    std::fprintf(stderr, "--media-out 不是可用目錄：%s\n", p.media_out_dir.c_str());
    return kExitUsage;
  }
  return kExitOk;
}

} // namespace cli::reqinput
