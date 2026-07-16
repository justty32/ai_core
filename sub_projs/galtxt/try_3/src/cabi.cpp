// cabi.cpp — C ABI 出口（llm_ask／llm_cancel／llm_phase）＋連線設定→http::Request。
//
// 這是整個 galtxt try_3 的對外面：對外就一支 llm_ask，一次吃 prompt＋schema＋tools＋media＋
// modalities＋stream，把四種能力併進「同一包 OpenAI 請求 JSON」再送出。組裝／解析／串流的重活
// 分在 cabi_request.cpp／cabi_response.cpp／cabi_stream.cpp（見 cabi_internal.hpp 的分工表），
// 本檔只留「膠水＋出口」：make_request（連線設定→http::Request）與三個 extern "C" 函數。
//
// 曾散在 L0 的四個獨立入口（ask／ask_tools／ask_vision／ask_json）已封存進 ../archived——那些
// 「可獨立呼叫的 C++ 函數」不再存在，請求組裝的知識只留 cabi_*.cpp 這一份（唯一真相源）。
//
// 只依賴：http（http.hpp，笨管子）＋各 cabi_*.cpp。phase／cancel 的邊界（只用 http 這層、未自持
// curl）：phase 粗粒度 IDLE→WAIT→STREAM→DONE／ERROR／CANCELLED（拿不到 CONNECT／UPLOAD 細分）；
// cancel 串流模式每塊查旗標能中途乾淨斷，非串流（http::request 阻塞無回呼）只在送出前查一次。

#include "cabi_internal.hpp"

#include <atomic>
#include <cstring>
#include <exception>
#include <string>

namespace cabi_impl {

// llm_client_t → http::Request（連線設定：endpoint 預設、Bearer、逾時；body 已組好）。
http::Request make_request(const llm_client_t *c, std::string body) {
  http::Request req;
  req.url = (c && c->endpoint) ? c->endpoint
                               : "http://localhost:1234/v1/chat/completions";
  req.method = "POST";
  req.headers = {"Content-Type: application/json"};
  if (c && c->api_key && *c->api_key)
    req.headers.push_back(std::string("Authorization: Bearer ") + c->api_key);
  req.body = std::move(body);
  req.timeout_ms = c ? c->timeout_ms : 0;
  return req;
}

} // namespace cabi_impl

// ══════════════════════════ C ABI 出口 ══════════════════════════
extern "C" {

void llm_cancel(llm_context_t *ctx) {
  if (ctx)
    std::atomic_ref<int>(ctx->cancel).store(1, std::memory_order_relaxed);
}

llm_phase_t llm_phase(const llm_context_t *ctx) {
  if (!ctx)
    return LLM_PHASE_IDLE;
  return static_cast<llm_phase_t>(
      std::atomic_ref<int>(const_cast<int &>(ctx->phase)).load(std::memory_order_relaxed));
}

llm_status_t llm_ask(llm_context_t *ctx, const llm_client_t *client,
                     const llm_request_t *req, const llm_handlers_t *handlers) {
  using namespace cabi_impl;
  set_phase(ctx, LLM_PHASE_IDLE);

  if (!req || !req->prompt) {
    if (handlers && handlers->on_error) {
      const char *m = "llm_ask: req/prompt 為空";
      handlers->on_error(m, std::strlen(m), handlers->error_user);
    }
    set_phase(ctx, LLM_PHASE_ERROR);
    return LLM_ERROR;
  }
  if (cancelled(ctx)) {
    set_phase(ctx, LLM_PHASE_CANCELLED);
    return LLM_CANCELLED;
  }

  try {
    std::string body = build_body(client, req);
    set_phase(ctx, LLM_PHASE_WAIT);

    if (req->stream)
      return do_stream(ctx, client, body, handlers);

    if (cancelled(ctx)) { // 非串流：http::request 阻塞不可中斷，只能送出前查
      set_phase(ctx, LLM_PHASE_CANCELLED);
      return LLM_CANCELLED;
    }
    http::Response resp = http::request(make_request(client, std::move(body)));
    guard_backend_error(resp.status, resp.body); // 後端錯誤不靜默吞成空字串
    dispatch_nonstream(resp.body, handlers);
    set_phase(ctx, LLM_PHASE_DONE);
    return LLM_OK;
  } catch (const std::exception &e) {
    std::string msg = e.what();
    if (handlers && handlers->on_error)
      handlers->on_error(msg.data(), msg.size(), handlers->error_user);
    set_phase(ctx, LLM_PHASE_ERROR);
    return LLM_ERROR;
  }
}

} // extern "C"
