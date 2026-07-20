// media.cpp — media.hpp 的實作：MIME 對照＋--image/--media 三分流取值。

#include "media.hpp"

#include <cctype>
#include <cstdio>

#include <glaze/glaze.hpp> // read_json（.json 描述子解析）

#include "cli_internal.hpp" // read_file

namespace cli::media {
namespace desc_impl { // 具名子 namespace（反射型不放匿名 namespace，見 gotchas）
// 預先算好的 media 描述子形狀：{"url":…} 或 {"mime":…,"data":"<base64>"}（多鍵容忍）。
struct MediaDescriptor {
  std::optional<std::string> url, mime, data;
};
inline constexpr glz::opts kLenient{.error_on_unknown_keys = false};
} // namespace desc_impl

static std::string to_lower(std::string s) {
  for (char &c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string mime_of(const std::string &path) {
  auto dot = path.find_last_of('.');
  std::string ext = to_lower(dot == std::string::npos ? "" : path.substr(dot + 1));
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

std::optional<llm::abi::MediaIn> build_media_item(const std::string &spec) {
  std::string low = to_lower(spec);
  // 1. data:／http(s):// URL → 直接當 media 的 url 送、不編碼（帶 url 就不帶 bytes）。
  if (spec.starts_with("data:") || low.starts_with("http://") || low.starts_with("https://"))
    return llm::abi::MediaIn{.url = spec};

  // 2. .json 副檔名（用副檔名判定、不嗅探內容）→ 讀檔當「預先算好的 media 描述子」直通。
  if (low.ends_with(".json")) {
    std::string body;
    try {
      body = cli::read_file(spec);
    } catch (const std::exception &) {
      std::fprintf(stderr, "讀不到檔案：%s（--image/--media 描述子）\n", spec.c_str());
      return std::nullopt;
    }
    desc_impl::MediaDescriptor d;
    if (glz::read<desc_impl::kLenient>(d, body)) {
      std::fprintf(stderr, "media 描述子 JSON 解析失敗（%s）\n", spec.c_str());
      return std::nullopt;
    }
    if (d.url && !d.url->empty())
      return llm::abi::MediaIn{.url = *d.url}; // 直通 url、不編碼
    if (d.mime && d.data)
      // base64 資料直通：包成 data URI（免解碼／重算，交後端／傳輸層還原）。
      return llm::abi::MediaIn{.url = "data:" + *d.mime + ";base64," + *d.data};
    std::fprintf(stderr,
                 "media 描述子形狀不符——需 {\"url\":…} 或 {\"mime\":…,\"data\":…}（%s）\n",
                 spec.c_str());
    return std::nullopt;
  }

  // 3. 其餘（二進位圖檔路徑）→ 讀檔走 bytes 路（mime 由副檔名推；ABI 自行編碼）。
  try {
    std::string bytes = cli::read_file(spec);
    return llm::abi::MediaIn{.url = "", .mime = mime_of(spec), .bytes = std::move(bytes)};
  } catch (const std::exception &) {
    std::fprintf(stderr, "讀不到檔案：%s（--image/--media）\n", spec.c_str());
    return std::nullopt;
  }
}

} // namespace cli::media
