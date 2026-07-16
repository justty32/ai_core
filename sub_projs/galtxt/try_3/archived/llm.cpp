// llm.cpp — llm.hpp 的實作。
//
// 「struct＋反射＝唯一真相源」的落地：送出的 JSON 由 ReqBody 定義、glaze 反射自動序列化；
// 收回的 JSON 由 RespBody/StreamChunk 反射自動解析。沒有 schema 表——加取樣旋鈕＝加欄位。
// std::optional 欄位在 nullopt 時被 glaze 自動略過（skip_null_members 預設開），所以「未設就不送」。

#include "llm.hpp"

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
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

// ── 護欄：後端錯誤不得被靜默吞成空字串 ──
// 真後端（LM Studio 模型載入失敗、雲端 401/429…）會回 OpenAI 風格 {"error":{"message":…}}
// ＋HTTP 4xx，其回應**無 choices**。若照舊「choices 空就回 {}」，呼叫端拿到空字串卻以為成功，
// 無法區分「模型回空」與「後端出錯」——對「笨模型＋護欄」是破洞。故解析前先攔錯、有錯就 throw
// （noexcept 的綁定橋會收斂成 VM 錯誤：Lua pcall 得 false＋訊息、s7 catch 得 llm-error）。
struct ErrDetail   { std::string message; std::optional<std::string> type; std::optional<std::string> code; };
struct ErrEnvelope { std::optional<ErrDetail> error; };

// 檢查 (status, raw) 是否代表後端錯誤：是則 throw runtime_error（帶後端訊息＋狀態碼）。
// 離線 file:// fixture 回 status=200 且無 error 物件 → 不受影響。
void throw_if_backend_error(int status, const std::string& raw) {
    ErrEnvelope env{};
    if (!glz::read<kLenient>(env, raw) && env.error) {   // 解得動且有 error 物件（message 最有用）
        throw std::runtime_error("後端錯誤 (HTTP " + std::to_string(status) + "): " + env.error->message);
    }
    if (status >= 400) {                                 // 無結構化 error 但 HTTP 失敗：帶狀態碼＋body 片段
        throw std::runtime_error("後端錯誤 (HTTP " + std::to_string(status) + "): " + raw.substr(0, 300));
    }
}

// 用 client 的連線設定，把已組好的 body JSON 包成 http::Request（POST＋標頭＋Bearer）。
http::Request build_request(const Client& client, std::string_view body_json) {
    http::Request req{
        .url = client.endpoint,
        .method = "POST",
        .headers = { "Content-Type: application/json" },
        .body = std::string(body_json),
    };
    if (!client.api_key.empty()) req.headers.push_back("Authorization: Bearer " + client.api_key);
    req.timeout_ms = client.timeout_ms;   // 0＝不設逾時；from_env() 給真後端一個寬鬆值
    return req;
}

// getenv 包一層預設值（比照 try_1 llm_entry.cpp 的 env() 慣例）。
std::string env_or(const char* key, const char* fallback) {
    const char* v = std::getenv(key);
    return v ? std::string(v) : std::string(fallback);
}

}  // namespace ask_impl
using namespace ask_impl;

// 共用傳輸（非串流）：組請求→送→回原始回應本體。傳輸失敗 throw；後端回錯（error JSON／4xx）也 throw。
// ★ 四個非串流入口（ask／ask_json／ask_tools／ask_vision）全走這裡，故護欄改一處四邊同時跟上。
std::string post(const Client& client, std::string_view request_json) {
    http::Response resp = http::request(build_request(client, request_json));
    throw_if_backend_error(resp.status, resp.body);   // 護欄：後端錯誤不靜默吞成空字串
    return std::move(resp.body);
}

Client from_env() {
    Client c;
    std::string base = env_or("AI_CORE_LLM_BASE_URL", "http://localhost:1234/v1");
    c.endpoint = base + "/chat/completions";
    c.model    = env_or("AI_CORE_LLM_MODEL", "local-model");
    c.api_key  = env_or("AI_CORE_LLM_API_KEY", "");
    c.timeout_ms = 120000;   // 2 分鐘：reasoning 模型單次可能要等數十秒，留寬裕空間
    return c;
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
    std::string raw_all;   // 護欄：串流下後端 4xx 錯誤（非 SSE、error JSON）也走 on_data 進這裡，事後攔錯用
    int status = http::stream(build_request(*this, *body), [&](std::string_view part) -> bool {
        raw_all.append(part);
        line_buf.append(part);
        size_t nl;
        while ((nl = line_buf.find('\n')) != std::string::npos) {
            std::string line = line_buf.substr(0, nl);
            line_buf.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();   // 去掉 CRLF 的 \r
            if (line.rfind("data: ", 0) != 0) continue;                  // 只認 data: 行
            std::string data = line.substr(6);
            if (data == "[DONE]") return true;                           // 串流結束，收工（true＝中止傳輸）
            StreamChunk chunk{};
            if (glz::read<kLenient>(chunk, data)) continue;              // 解不動這筆就跳過
            if (chunk.choices.empty()) continue;
            const std::string& piece = chunk.choices[0].delta.content;
            if (piece.empty()) continue;
            answer += piece;
            if (on_delta && on_delta(piece)) return true;               // 呼叫端喊停（on_delta 回 true＝中止）
        }
        return false;                                                   // 這批處理完，繼續收下一塊
    });
    throw_if_backend_error(status, raw_all);   // 護欄：串流下後端出錯（4xx＋error JSON）不靜默回空
    return answer;
}

}  // namespace llm
