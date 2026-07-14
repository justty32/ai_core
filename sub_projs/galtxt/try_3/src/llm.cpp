// llm.cpp — llm.hpp 的實作。
//
// 「struct＋反射＝唯一真相源」的落地：送出的 JSON 由 ReqBody 定義、glaze 反射自動序列化；
// 收回的 JSON 由 RespBody/StreamChunk 反射自動解析。沒有 schema 表——加取樣旋鈕＝加欄位。
// std::optional 欄位在 nullopt 時被 glaze 自動略過（skip_null_members 預設開），所以「未設就不送」。

#include "llm.hpp"

#include <optional>
#include <vector>

#include <glaze/glaze.hpp>   // 反射式 JSON↔struct
#include "http.hpp"          // native HTTP 傳輸：request / stream

namespace llm {

// 具名的內部 namespace（不用匿名）：這些 struct 名（ReqBody/RespBody…）在 llm_tool/llm_media
// 也各有一份，若用匿名 namespace，GCC 一律 mangle 成 _GLOBAL__N_1、glaze 的 quoted_key_v
// COMDAT section 會跨 TU 撞名（大小不同→折疊後給錯 JSON 鍵）。給每個 TU 唯一 namespace 名即避開。
namespace ask_impl {

// ── 送出：OpenAI chat completions 請求體（欄位名即送出的 JSON 鍵；optional 未設則略過）
struct ReqMessage { std::string role; std::string content; };
struct ReqBody {
    std::optional<std::string> model;
    std::vector<ReqMessage> messages;
    std::optional<float> temperature;
    std::optional<float> top_p;
    bool stream = false;                    // 這個總是送：明確標示要不要串流
    std::optional<int>   max_tokens;
    std::optional<float> presence_penalty;
    std::optional<float> frequency_penalty;
    std::optional<int>   seed;
};

// ── 收回（非串流）：只挑要的欄位，其餘靠 error_on_unknown_keys=false 忽略
struct RespMessage { std::string role; std::string content; };
struct RespChoice  { RespMessage message; };
struct RespBody    { std::vector<RespChoice> choices; };

// ── 收回（串流）：SSE 每筆 data: 的 delta
struct Delta        { std::string content; };
struct StreamChoice { Delta delta; };
struct StreamChunk  { std::vector<StreamChoice> choices; };

constexpr glz::opts kLenient{ .error_on_unknown_keys = false };  // 後端回應欄位多，寬鬆解析

// 用 client 的連線設定，把已組好的 body JSON 包成 http::Request（POST＋標頭＋Bearer）。
http::Request build_request(const Client& client, std::string_view body_json) {
    http::Request req{
        .url = client.endpoint,
        .method = "POST",
        .headers = { "Content-Type: application/json" },
        .body = std::string(body_json),
    };
    if (!client.api_key.empty()) req.headers.push_back("Authorization: Bearer " + client.api_key);
    return req;
}

}  // namespace ask_impl
using namespace ask_impl;

// 共用傳輸（非串流）：組請求→送→回原始回應本體。傳輸失敗 throw。
std::string post(const Client& client, std::string_view request_json) {
    http::Response resp = http::request(build_request(client, request_json));
    return std::move(resp.body);
}

std::string Client::ask(std::string_view prompt, bool stream, OnDelta on_delta) {
    // 1) 用 struct 組請求，glaze 反射成 JSON（不手寫映射、不查 schema 表）。
    //    取樣屬性用 apply_sampling 灌入；optional 保持 nullopt 就不會出現在 JSON 裡。
    ReqBody payload;
    payload.messages = { { "user", std::string(prompt) } };
    payload.stream = stream;
    apply_sampling(*this, payload);
    auto body = glz::write_json(payload);
    if (!body) return {};   // 序列化失敗（理論上不會）

    // 2a) 非串流：共用 post 一次收完 → 反射解析 → 取第一則答覆內容
    if (!stream) {
        std::string raw = post(*this, *body);
        RespBody parsed{};
        auto ec = glz::read<kLenient>(parsed, raw);
        if (ec || parsed.choices.empty()) return {};
        return parsed.choices[0].message.content;
    }

    // 2b) 串流：http 只逐塊吐 raw bytes，SSE 拆框在這層做（笨管子／上層分工）。
    //     跨塊維持一個行緩衝（網路分塊不對齊行邊界），逐行解 data:，累積並餵 on_delta。
    std::string answer;
    std::string line_buf;
    http::stream(build_request(*this, *body), [&](std::string_view part) -> bool {
        line_buf.append(part);
        size_t nl;
        while ((nl = line_buf.find('\n')) != std::string::npos) {
            std::string line = line_buf.substr(0, nl);
            line_buf.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();   // 去掉 CRLF 的 \r
            if (line.rfind("data: ", 0) != 0) continue;                  // 只認 data: 行
            std::string data = line.substr(6);
            if (data == "[DONE]") return false;                          // 串流結束，收工
            StreamChunk chunk{};
            if (glz::read<kLenient>(chunk, data)) continue;              // 解不動這筆就跳過
            if (chunk.choices.empty()) continue;
            const std::string& piece = chunk.choices[0].delta.content;
            if (piece.empty()) continue;
            answer += piece;
            if (on_delta && !on_delta(piece)) return false;             // 呼叫端喊停
        }
        return true;
    });
    return answer;
}

}  // namespace llm
