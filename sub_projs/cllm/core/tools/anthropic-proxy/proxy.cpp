// proxy.cpp — anthropic-proxy 執行檔：讓只會講 OpenAI＋Bearer 的 cllm 打得到 Anthropic。
//
// 入站用共用 httpd（tools/common）收 cllm 的 OpenAI 請求；出站重用 cllm 的 http（src/http）
// 打 Anthropic Messages。翻譯全在 translate。無狀態：key 從 cllm 送來的 Authorization 當場
// 轉手成 x-api-key，代理自己不存不落地。
//
//   anthropic-proxy [--host 127.0.0.1] [--port 8787] [--base https://api.anthropic.com]
//   env：ANTHROPIC_BASE_URL / ANTHROPIC_VERSION / ANTHROPIC_MAX_TOKENS / PORT

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>

#include "http.hpp"      // cllm 出站笨管子（src/）
#include "httpd.hpp"     // 共用入站 server（tools/common/）
#include "translate.hpp"

namespace {

std::string env_or(const char *k, std::string d) {
  const char *v = std::getenv(k);
  return v && *v ? std::string(v) : d;
}

std::string bearer(const httpd::Request &req) {
  std::string a = req.header("Authorization");
  if (a.rfind("Bearer ", 0) == 0) return a.substr(7);
  return a;
}

std::string dump(axl::J &j) {
  std::string s;
  (void)glz::write_json(j, s);
  return s;
}

std::string err_json(const std::string &msg) {
  return "{\"error\":{\"message\":\"" + msg + "\"}}";
}

// 從 SSE line buffer 抽出完整行，翻譯後 write_chunk；殘料留在 buf。
void drain_sse(std::string &buf, axl::StreamXlate &xlate, httpd::Responder &res) {
  size_t nl;
  while ((nl = buf.find('\n')) != std::string::npos) {
    std::string line = buf.substr(0, nl);
    buf.erase(0, nl + 1);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("data:", 0) != 0) continue;
    std::string data = line.substr(5);
    while (!data.empty() && data.front() == ' ') data.erase(data.begin());
    if (data.empty() || data == "[DONE]") continue;
    axl::J evt;
    if (glz::read_json(evt, data)) continue;  // 解不動就跳過
    for (const std::string &out : xlate.feed(evt)) res.write_chunk(out);
  }
}

struct Config {
  std::string base = env_or("ANTHROPIC_BASE_URL", "https://api.anthropic.com");
  std::string version = env_or("ANTHROPIC_VERSION", "2023-06-01");
  int max_tokens = std::atoi(env_or("ANTHROPIC_MAX_TOKENS",
                                    std::to_string(axl::kDefaultMaxTokens).c_str())
                                 .c_str());
};

void handle(const httpd::Request &hreq, httpd::Responder &res, const Config &cfg) {
  if (hreq.method != "POST") {
    res.send_json(405, err_json("只收 POST"));
    return;
  }
  axl::J oai;
  if (glz::read_json(oai, hreq.body) || !oai.is_object()) {
    res.send_json(400, err_json("壞掉的請求 JSON"));
    return;
  }
  std::string key = bearer(hreq);
  if (key.empty()) {
    res.send_json(401, err_json("缺 Authorization: Bearer <anthropic-key>——"
                                "把 sk-ant-... 填進 cllm config 的 api_key。"));
    return;
  }

  axl::J anth = axl::to_anthropic(oai, cfg.max_tokens);
  bool want_stream = anth.is_object() && anth.contains("stream") &&
                     anth["stream"].is_boolean() && anth["stream"].get_boolean();

  http::Request out;
  out.url = cfg.base + "/v1/messages";
  out.method = "POST";
  out.headers = {"content-type: application/json", "x-api-key: " + key,
                 "anthropic-version: " + cfg.version};
  out.body = dump(anth);

  if (!want_stream) {
    http::Response up = http::request(out);
    axl::J parsed;
    bool ok = !glz::read_json(parsed, up.body);
    if (up.status >= 400 || (ok && parsed.is_object() && parsed.contains("error"))) {
      axl::J oe = ok ? axl::err_to_openai(parsed) : axl::J{};
      std::string body = ok ? dump(oe) : err_json("上游錯誤 HTTP " + std::to_string(up.status));
      res.send_json(up.status >= 400 ? up.status : 502, body);
      return;
    }
    axl::J oo = axl::from_anthropic(parsed);
    res.send_json(200, dump(oo));
    return;
  }

  // 串流：Anthropic 上游可能回 SSE（成功）或 JSON 錯誤體（4xx）。先偷看首個非空白字元決定：
  // '{' → 錯誤體（還沒送標頭，可回正常錯誤）；否則 → SSE（begin_chunked 開串）。
  axl::StreamXlate xlate;
  std::string raw, linebuf;
  bool decided = false, is_sse = false;
  int status = http::stream(out, [&](std::string_view part) -> bool {
    raw.append(part);
    if (!decided) {
      size_t i = 0;
      while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
      if (i >= raw.size()) return false;  // 全空白，再等
      decided = true;
      is_sse = raw[i] != '{';
      if (is_sse) {
        res.begin_chunked(200, "text/event-stream");
        linebuf = raw;  // 已含當前 part
        drain_sse(linebuf, xlate, res);
      }
    } else if (is_sse) {
      linebuf.append(part);
      drain_sse(linebuf, xlate, res);
    }
    return false;
  });

  if (is_sse) {
    res.write_chunk("data: [DONE]\n\n");
    res.end_chunked();
  } else {
    axl::J parsed;
    bool ok = !glz::read_json(parsed, raw);
    axl::J oe = ok ? axl::err_to_openai(parsed) : axl::J{};
    std::string body = ok ? dump(oe) : err_json("上游錯誤 HTTP " + std::to_string(status));
    res.send_json(status >= 400 ? status : 502, body);
  }
}

}  // namespace

int main(int argc, char **argv) {
  std::string host = "127.0.0.1";
  int port = std::atoi(env_or("PORT", "8787").c_str());
  Config cfg;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
    if (a == "--host") host = next();
    else if (a == "--port") port = std::atoi(next().c_str());
    else if (a == "--base") cfg.base = next();
    else if (a == "-h" || a == "--help") {
      std::cerr << "用法: anthropic-proxy [--host H] [--port P] [--base URL]\n";
      return 0;
    }
  }

  try {
    httpd::Server server(host, port);
    std::cerr << "anthropic-proxy 監聽 http://" << host << ":" << server.port()
              << " → " << cfg.base << "（version " << cfg.version << "）\n";
    std::cerr << "cllm config：endpoint=http://" << host << ":" << server.port()
              << "/v1/chat/completions、model=<anthropic id>、api_key=<sk-ant-...>\n";
    server.serve_forever(
        [&cfg](const httpd::Request &req, httpd::Responder &res) { handle(req, res, cfg); });
  } catch (const std::exception &e) {
    std::cerr << "錯誤：" << e.what() << "\n";
    return 1;
  }
  return 0;
}
