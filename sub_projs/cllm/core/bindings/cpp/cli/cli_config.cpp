// cli_config.cpp — cli_config.hpp 的實作：config 檔三層來源的前二層。
//   glaze 直接反射灌入 llm::abi::Client（未列鍵寬鬆忽略，與 core-py 一致）。

#include "cli_config.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <glaze/glaze.hpp> // read / format_error（config 反射灌入 Client）

#include "cli_internal.hpp" // kExitOk / kExitUsage / kConfigEnvVar / read_file

namespace cli::config {
namespace {
// 未列鍵寬鬆忽略（error_on_unknown_keys=false）：對齊 core-py 的 lenient；壞 JSON 仍照樣報錯。
inline constexpr glz::opts kLenient{.error_on_unknown_keys = false};

std::string default_config_path() {
  const char *home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return std::string(home) + "/.config/llm/config.json";
}
} // namespace

int load_into(llm::Client &client, bool has_config, const std::string &config_path) {
  // 路徑解析：--config ＞ env ＞ 家目錄。前二者「明指」（讀不到＝用法錯），後者「探測」（靜默略過）。
  std::string cfg_path;
  bool named = false;
  if (has_config) {
    cfg_path = config_path;
    named = true;
  } else if (const char *env = std::getenv(kConfigEnvVar); env && *env) {
    cfg_path = env;
    named = true;
  } else {
    cfg_path = default_config_path();
  }
  if (cfg_path.empty())
    return kExitOk;

  std::string body;
  try {
    body = read_file(cfg_path);
  } catch (const std::exception &) {
    if (named) { // 明指卻讀不到＝用法錯（點名是誰指的路）
      std::fprintf(stderr, "讀不到檔案：%s（%s 指定的 config 檔）\n", cfg_path.c_str(),
                   has_config ? "--config" : kConfigEnvVar);
      return kExitUsage;
    }
    return kExitOk; // 探測路徑讀不到＝沒設定檔，靜默用預設
  }

  // 反射灌入 abi::Client（llm::Client 無新增資料成員，反射走基底型別）。
  llm::abi::Client &base = client;
  if (auto ec = glz::read<kLenient>(base, body)) {
    std::fprintf(stderr, "config JSON 解析失敗（%s）：%s\n", cfg_path.c_str(),
                 glz::format_error(ec, body).c_str());
    return kExitUsage;
  }
  return kExitOk;
}

} // namespace cli::config
