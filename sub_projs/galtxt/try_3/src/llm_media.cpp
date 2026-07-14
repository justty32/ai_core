// llm_media.cpp — llm_media.hpp 的實作。
//
// 送出：user 訊息的 content 是異質陣列（文字格＋圖片格混排）。glaze 的 vector 要同型，故把
//   每一格各自序列化成 JSON、包成 glz::raw_json，湊成 vector<glz::raw_json> 當 content。
// 收回：跟一般回應一樣取 choices[0].message.content。

#include "llm_media.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

#include <glaze/glaze.hpp>

namespace llm {

// ── 建圖：本地檔 → base64 data URI ──
Image image_from_file(std::string_view path, std::string_view mime) {
    std::ifstream f(std::string(path), std::ios::binary);
    if (!f) throw std::runtime_error(std::string("llm_media: 開不了圖檔：") + std::string(path));
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string bytes = ss.str();
    std::string b64 = glz::write_base64(bytes);
    return Image{ "data:" + std::string(mime) + ";base64," + b64 };
}

Image image_from_url(std::string url) {
    return Image{ std::move(url) };
}

// 具名 namespace（非匿名）：避免與 llm.cpp／llm_tool.cpp 同名 struct 的 glaze COMDAT section
// 跨 TU 撞名（GCC 匿名 namespace 一律 mangle 成 _GLOBAL__N_1）。
namespace media_impl {

// content 各格：文字格、圖片格（各自序列化，故不需共存同一 struct）
struct TextPart { std::string type = "text"; std::string text; };
struct ImageUrl { std::string url; };
struct ImagePart { std::string type = "image_url"; ImageUrl image_url; };

// ── 送出的請求體：content 是異質陣列 → vector<raw_json> ──
struct MediaMessage {
    std::string role = "user";
    std::vector<glz::raw_json> content;
};
struct ReqBody {
    std::optional<std::string> model;
    std::vector<MediaMessage> messages;
    std::optional<float> temperature;
    std::optional<float> top_p;
    std::optional<int>   max_tokens;
    std::optional<float> presence_penalty;
    std::optional<float> frequency_penalty;
    std::optional<int>   seed;
};

// ── 收回：取答覆文字 ──
struct RespMessage { std::string content; };
struct RespChoice  { RespMessage message; };
struct RespBody    { std::vector<RespChoice> choices; };

constexpr glz::opts kLenient{ .error_on_unknown_keys = false };

// 把一格 part 序列化成 raw_json（失敗回空物件）
template <class Part>
glz::raw_json part_json(const Part& p) {
    auto s = glz::write_json(p);
    return glz::raw_json{ s ? std::move(*s) : std::string("{}") };
}

}  // namespace media_impl
using namespace media_impl;

std::string ask_vision(const Client& client,
                       std::string_view text,
                       const std::vector<Image>& images) {
    // 組多模態 content：先一格文字，再每張圖一格
    MediaMessage msg;
    msg.content.push_back(part_json(TextPart{ .text = std::string(text) }));
    for (const auto& img : images) {
        msg.content.push_back(part_json(ImagePart{ .image_url = { img.url } }));
    }

    ReqBody payload;
    payload.messages = { std::move(msg) };
    apply_sampling(client, payload);

    auto body = glz::write_json(payload);
    if (!body) return {};

    std::string raw = post(client, *body);   // 共用 llm::post
    RespBody parsed{};
    if (glz::read<kLenient>(parsed, raw) || parsed.choices.empty()) return {};
    return parsed.choices[0].message.content;
}

}  // namespace llm
