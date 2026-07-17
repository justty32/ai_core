// store.cpp — 狀態層實作：路徑、JSON 檔讀寫（0600）、patch cllm config。

#include "store.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace login::store {

std::string home_dir() {
#ifdef _WIN32
  const char *h = std::getenv("USERPROFILE");
#else
  const char *h = std::getenv("HOME");
#endif
  return h ? std::string(h) : std::string();
}

std::string default_path(const std::string &filename) {
  std::string h = home_dir();
  if (h.empty()) return filename;
  return h + "/.config/llm/" + filename;
}

J read_json(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) throw std::runtime_error("讀不到檔：" + path);
  std::stringstream ss;
  ss << f.rdbuf();
  std::string body = ss.str();
  J j;
  if (glz::read_json(j, body)) throw std::runtime_error("JSON 解析失敗：" + path);
  return j;
}

void write_file(const std::string &path, const std::string &content, bool mode0600) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) throw std::runtime_error("寫不了檔：" + path);
  f << content;
  f.close();
#ifndef _WIN32
  if (mode0600) ::chmod(path.c_str(), 0600);
#else
  (void)mode0600;
#endif
}

J make_token_record(J &raw, long now_unix) {
  J rec;
  rec = J::object_t{};
  auto copy_str = [&](const char *k) {
    if (raw.is_object() && raw.contains(k) && raw[k].is_string()) rec[k] = raw[k];
  };
  copy_str("access_token");
  copy_str("refresh_token");
  // expires_in（秒）→ expires_at（絕對 unix 秒）
  if (raw.is_object() && raw.contains("expires_in") && raw["expires_in"].is_number()) {
    double at = static_cast<double>(now_unix) + raw["expires_in"].get_number();
    rec["expires_at"] = at;
  }
  return rec;
}

bool is_expired(J &rec, long now_unix) {
  if (!rec.is_object() || !rec.contains("expires_at") || !rec["expires_at"].is_number())
    return false;  // 不過期型（如 OpenRouter key）
  return rec["expires_at"].get_number() <= static_cast<double>(now_unix) + 60;  // 60s skew
}

std::string patch_cllm(const std::string &config_path, const std::string &token,
                       const std::string &endpoint, const std::string &model) {
  J cfg;
  cfg = J::object_t{};
  // 已有 config 就讀進來（保留其餘鍵）；沒有就從空物件起
  std::ifstream f(config_path, std::ios::binary);
  if (f) {
    std::stringstream ss;
    ss << f.rdbuf();
    std::string body = ss.str();
    if (!body.empty() && glz::read_json(cfg, body))
      throw std::runtime_error("既有 config 解析失敗：" + config_path);
    if (!cfg.is_object()) cfg = J::object_t{};
  }
  cfg["api_key"] = token;                          // 只動 cllm 認得的鍵
  if (!endpoint.empty()) cfg["endpoint"] = endpoint;
  if (!model.empty()) cfg["model"] = model;
  std::string out;
  (void)glz::write_json(cfg, out);
  write_file(config_path, out, false);
  return config_path;
}

}  // namespace login::store
