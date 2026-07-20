#pragma once
// llm.hpp — cllm C++ 便利層（薄鏡像 llm::abi 之上的「好用包裝」）。header-only、零額外依賴。
//
// 定位：cabi.hpp（llm::abi）刻意「不聚合、不拋例外、輸出全走 handlers」；本層補上人體工學——
//   · 聚合：ask() 回完整答案（文字／tool_calls／media 收成 Reply），不用自己寫 accumulator
//   · 錯誤統一：所有 API 一律回 std::expected<T, Error>（C++23）；不混 throw／空字串／nullopt
//     （舊 archived 富 API 三套失敗規則混雜的教訓）。要一行爽寫就 `.value()`（就地拋）。
//   · 串流糖：opts.on_delta 逐段回呼（回 true 中止），完整文字照樣聚合回來
//   · media helpers：media_from_file()（副檔名猜 mime）／media_from_url()
// ★ glaze 反射糖（ask_as<T>／make_tool<Args>／args_as<Args>）在隔壁 llm_reflect.hpp——
//   那層要 glaze 標頭；本檔只要 <cllm/cabi.hpp>，維持「bindings 只消費 C ABI」的輕。
//
// 編：g++ -std=c++23 x.cpp $(pkg-config --cflags --libs cllm)
// 用：
//   llm::Client c;                        // 繼承 abi::Client：endpoint／api_key／取樣 optional 照設
//   auto a = c.ask("你好");               // Result<std::string>
//   if (a) std::fputs(a->c_str(), stdout); else std::fputs(a.error().message.c_str(), stderr);
//   c.ask("數到十", {.stream = true, .on_delta = [](auto sv){ …逐段…; return false; }});

#include <cllm/cabi.hpp>

#include <cctype>
#include <expected>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace llm {

// 統一錯誤型：所有便利層 API 的失敗都長這樣。status 對應 abi::Status（Cancelled＝ctx.cancel()、
// Error＝傳輸/後端失敗），message 來自 on_error（後端錯誤原文）；partial＝失敗/取消前已聚合到的
// 文字（不白丟）。註：on_delta 回 true 是「主動收工」不是失敗——那條路 ask 回 Ok。
struct Error {
  abi::Status status = abi::Status::Error;
  std::string message;
  std::string partial;
};

template <class T>
using Result = std::expected<T, Error>;

// 一次發問的完整收成（三類輸出全聚合）。只要文字就用 ask() 的 string 版，不必碰這個。
struct Reply {
  std::string text;
  std::vector<abi::ToolCall> tool_calls;
  std::vector<abi::MediaOut> media;
};

// ask 的便利選項（Request 之外的「收法」）。
struct AskOpts {
  bool stream = false;                            // 疊加到 Request.stream（或起來，不覆蓋）
  std::function<bool(std::string_view)> on_delta; // 逐段文字回呼；回 true＝主動收工（ask 回 Ok、文字聚合到當下為止）
  abi::Context *ctx = nullptr;                    // 跨 thread cancel()（→ Cancelled）／phase()
};

// 便利 Client：設定欄位承襲 abi::Client（endpoint／api_key／取樣 optional…），ask 換成聚合版。
// 本層的 ask 遮蔽了基底 handler 版；要原始 handler 級控制走 ask_raw()。
class Client : public abi::Client {
public:
  // 全功能：Request 任意組合（schema／tools／media／modalities）→ 收成 Reply。
  Result<Reply> ask(const abi::Request &req, const AskOpts &opts = {}) const {
    abi::Request r = req;
    r.stream = r.stream || opts.stream;

    Reply reply;
    std::string err_msg;
    abi::Handlers h;
    h.on_text = [&](std::string_view sv) {
      reply.text.append(sv);
      return opts.on_delta ? opts.on_delta(sv) : false;
    };
    h.on_tool = [&](const abi::ToolCall &c) {
      reply.tool_calls.push_back(c);
      return false;
    };
    h.on_media = [&](const abi::MediaOut &m) {
      reply.media.push_back(m);
      return false;
    };
    h.on_error = [&](std::string_view sv) { err_msg.assign(sv); };

    abi::Status st = abi::Client::ask(r, h, opts.ctx);
    if (st == abi::Status::Ok) return reply;
    if (err_msg.empty())
      err_msg = st == abi::Status::Cancelled ? "已取消（ctx.cancel()）"
                                             : "後端失敗（無錯誤訊息）";
    return std::unexpected(Error{st, std::move(err_msg), std::move(reply.text)});
  }

  // 主糖：一句話問、拿完整答案字串。串流照樣聚合（on_delta 逐段、回傳仍是整段）。
  Result<std::string> ask(std::string_view prompt, const AskOpts &opts = {}) const {
    return ask(abi::Request{.prompt = std::string(prompt)}, opts)
        .transform([](Reply &&r) { return std::move(r.text); });
  }

  // 工具糖：送 tools、收模型要求的呼叫。C ABI 是單輪——執行工具與是否回送由呼叫端決定。
  Result<std::vector<abi::ToolCall>> ask_tools(std::string_view prompt,
                                               std::vector<abi::ToolDef> tools,
                                               const AskOpts &opts = {}) const {
    return ask(abi::Request{.prompt = std::string(prompt), .tools = std::move(tools)}, opts)
        .transform([](Reply &&r) { return std::move(r.tool_calls); });
  }

  // 原始介面直通（＝abi::Client::ask 原樣，被上面的聚合版遮蔽時走這裡）。
  abi::Status ask_raw(const abi::Request &req, const abi::Handlers &h = {},
                      abi::Context *ctx = nullptr) const {
    return abi::Client::ask(req, h, ctx);
  }
};

// —— media helpers ——

// 從副檔名猜 mime（常見影音圖＋pdf；猜不到回空字串）。
inline std::string guess_mime(std::string_view path) {
  auto dot = path.rfind('.');
  if (dot == std::string_view::npos) return "";
  std::string ext(path.substr(dot + 1));
  for (char &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  struct Row { const char *ext, *mime; };
  static constexpr Row table[] = {
      {"png", "image/png"},   {"jpg", "image/jpeg"},  {"jpeg", "image/jpeg"},
      {"gif", "image/gif"},   {"webp", "image/webp"}, {"bmp", "image/bmp"},
      {"svg", "image/svg+xml"}, {"mp3", "audio/mpeg"}, {"wav", "audio/wav"},
      {"ogg", "audio/ogg"},   {"flac", "audio/flac"}, {"mp4", "video/mp4"},
      {"webm", "video/webm"}, {"pdf", "application/pdf"},
  };
  for (const Row &r : table)
    if (ext == r.ext) return r.mime;
  return "";
}

// 讀本地檔成 MediaIn（bytes 路線，免 base64）。mime 空＝按副檔名猜；猜不到或讀不到＝Error。
inline Result<abi::MediaIn> media_from_file(const std::string &path, std::string mime = "") {
  if (mime.empty()) mime = guess_mime(path);
  if (mime.empty())
    return std::unexpected(Error{.message = "猜不出 mime：" + path + "（請明給 mime 參數）"});
  std::ifstream f(path, std::ios::binary);
  if (!f) return std::unexpected(Error{.message = "讀不到檔案：" + path});
  std::string bytes{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
  return abi::MediaIn{.mime = std::move(mime), .bytes = std::move(bytes)};
}

// URL 路線（https:// 或 data URI）。mime 可省——data URI 自帶、外部 URL 後端自己抓。
inline abi::MediaIn media_from_url(std::string url, std::string mime = "") {
  return abi::MediaIn{.url = std::move(url), .mime = std::move(mime)};
}

} // namespace llm
