// cli_config.cpp — cli_config.hpp 的實作：config 檔三層來源的前二層 + 檔案/mime/工具定義/媒體落檔小工具。

#include "cli_config.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <glaze/glaze.hpp> // read_json / format_error（config 反射灌入 Client）
#include "cabi.hpp"        // llm::abi::Client
#include "cli_internal.hpp" // kExitOk / kExitUsage / kConfigEnvVar

namespace cli::config {

bool read_file(const std::string &path, std::string &out, std::string &err) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    err = "讀不到檔案：" + path;
    return false;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

std::string mime_of(const std::string &path) {
  auto dot = path.find_last_of('.');
  std::string ext = (dot == std::string::npos) ? "" : path.substr(dot + 1);
  for (char &c : ext)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (ext == "png") return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "gif") return "image/gif";
  if (ext == "webp") return "image/webp";
  if (ext == "wav") return "audio/wav";
  if (ext == "mp3") return "audio/mpeg";
  return "application/octet-stream";
}

std::string ext_of(const std::string &mime) {
  if (mime == "image/png") return "png";
  if (mime == "image/jpeg") return "jpg";
  if (mime == "image/gif") return "gif";
  if (mime == "image/webp") return "webp";
  if (mime == "audio/wav") return "wav";
  if (mime == "audio/mpeg") return "mp3";
  return "bin";
}

namespace tool_impl { // 具名子 namespace（本專案不用匿名 namespace 放反射型，見 gotchas）
// --tool 定義檔的形狀：parameters 用 raw_json 原樣收（它是「參數的 JSON Schema」，不重組）。
struct ToolDefFile {
  std::string name;
  std::string description{};
  glz::raw_json parameters{};
};
inline constexpr glz::opts kLenient{.error_on_unknown_keys = false}; // 容忍多餘鍵（向前相容）
} // namespace tool_impl

bool load_tool_def(const std::string &path, llm::abi::ToolDef &out, std::string &err) {
  std::string body;
  if (!read_file(path, body, err))
    return false;
  tool_impl::ToolDefFile def;
  if (auto ec = glz::read<tool_impl::kLenient>(def, body)) {
    err = "工具定義 JSON 解析失敗（" + path + "）：" + glz::format_error(ec, body);
    return false;
  }
  if (def.name.empty() || def.parameters.str.empty()) {
    err = "工具定義缺 name 或 parameters（" + path + "）";
    return false;
  }
  out = llm::abi::ToolDef{.name = std::move(def.name),
                          .description = std::move(def.description),
                          .parameters = std::move(def.parameters.str)};
  return true;
}

bool save_media(const std::string &dir, int n, const llm::abi::MediaOut &media,
                std::string &path_out, std::string &err) {
  path_out = dir + "/llm-media-" + std::to_string(n) + "." + ext_of(media.mime);
  std::ofstream f(path_out, std::ios::binary | std::ios::trunc);
  if (!f || !f.write(media.bytes.data(), static_cast<std::streamsize>(media.bytes.size()))) {
    err = "媒體落檔失敗：" + path_out;
    return false;
  }
  return true;
}

std::string default_config_path() {
  const char *home = std::getenv("HOME");
  if (!home || !*home)
    return {};
  return std::string(home) + "/.config/llm/config.json";
}

int load_into(llm::abi::Client &client, bool has_config, const std::string &config_path) {
  // 路徑解析：--config ＞ env ＞ 家目錄。前二者「明指」，後者「探測」。
  std::string cfg_path;
  bool cfg_named = false; // 明指（flag/env）＝必須成功載入
  if (has_config) {
    cfg_path = config_path;
    cfg_named = true;
  } else if (const char *env = std::getenv(kConfigEnvVar); env && *env) {
    cfg_path = env;
    cfg_named = true;
  } else {
    cfg_path = default_config_path();
  }
  if (cfg_path.empty())
    return kExitOk;

  std::string body, err;
  if (read_file(cfg_path, body, err)) {
    if (auto ec = glz::read_json(client, body)) {
      std::fprintf(stderr, "config JSON 解析失敗（%s）：%s\n", cfg_path.c_str(),
                   glz::format_error(ec, body).c_str());
      return kExitUsage;
    }
  } else if (cfg_named) { // 明指卻讀不到＝用法錯（點名是誰指的路）
    std::fprintf(stderr, "%s（%s 指定的 config 檔）\n", err.c_str(),
                 has_config ? "--config" : kConfigEnvVar);
    return kExitUsage;
  }
  // 探測路徑讀不到＝沒設定檔，靜默用預設
  return kExitOk;
}

} // namespace cli::config
