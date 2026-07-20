// argv.cpp — argv.hpp 的實作：argv 掃描（對齊 argv.py 的 parse_argv／cli.cpp 解析段）。

#include "argv.hpp"

#include <cstdio>

#include "cli_flags.hpp"    // client_flags / print_usage
#include "cli_internal.hpp" // kExitOk / kExitUsage

namespace cli::argv {

ParsedArgs parse_argv(const std::vector<std::string> &args) {
  // 合法反射旗標表：--flag → Client 欄位名。
  std::map<std::string, std::string> flag_to_field;
  for (const auto &fi : flags::client_flags())
    flag_to_field[fi.flag] = fi.field;

  ParsedArgs p;
  std::size_t i = 1, n = args.size();
  bool no_more_flags = false; // 見到 "--" 後，其餘 argv 一律當 prompt（unix 慣例）

  // 吃下一個 argv 當旗標的值；缺值印錯回 false（呼叫端據此回 kExitUsage）。
  auto need_value = [&](const std::string &flag, std::string &dst) -> bool {
    if (i + 1 >= n) {
      std::fprintf(stderr, "%s 缺少值（llm-cpp --help 看用法）\n", flag.c_str());
      return false;
    }
    dst = args[++i];
    return true;
  };
  auto usage_stop = [&]() { p.stop = true; p.exit_code = kExitUsage; return p; };

  for (; i < n; ++i) {
    const std::string &a = args[i];
    if (no_more_flags) { p.prompt_parts.push_back(a); continue; }
    if (a == "--") { no_more_flags = true; continue; }
    if (a == "--help" || a == "-h") { flags::print_usage(); p.stop = true; p.exit_code = kExitOk; return p; }
    if (a == "--stream") { p.stream = true; continue; }
    std::string v;
    if (a == "--image" || a == "--media") {
      if (!need_value(a, v)) return usage_stop();
      p.media_specs.push_back(v); continue;
    }
    if (a == "--schema") {
      if (!need_value(a, p.schema_text)) return usage_stop();
      p.has_schema = true; continue;
    }
    if (a == "--system") {
      if (!need_value(a, p.system_text)) return usage_stop();
      p.has_system = true; continue;
    }
    if (a == "--config") {
      if (!need_value(a, p.config_path)) return usage_stop();
      p.has_config = true; continue;
    }
    if (a == "--tool") {
      if (!need_value(a, v)) return usage_stop();
      p.tool_specs.push_back(v); continue;
    }
    if (a == "--modality") {
      if (!need_value(a, v)) return usage_stop();
      p.modality_specs.push_back(v); continue;
    }
    if (a == "--media-out") {
      if (!need_value(a, p.media_out_dir)) return usage_stop();
      continue;
    }
    if (auto it = flag_to_field.find(a); it != flag_to_field.end()) {
      if (!need_value(a, v)) return usage_stop();
      p.raw_values[it->second] = v; continue;
    }
    if (!a.empty() && a[0] == '-' && a != "-") { // 「-」＝stdin 佔位符；其餘 '-' 開頭＝未知旗標
      std::fprintf(stderr, "未知旗標：%s（llm-cpp --help 看用法）\n", a.c_str());
      return usage_stop();
    }
    p.prompt_parts.push_back(a); // 位置參數
  }
  return p;
}

} // namespace cli::argv
