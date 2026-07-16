#pragma once
// cabi_internal.hpp — galtxt try_3 C ABI 實作的「內部」共用宣告（非對外介面）。
//
// cabi.cpp 曾把 request／response／stream／orchestrator 全塞一檔（太肥）。拆成關注點各一檔後，
// 彼此共用的東西集中在這裡：context 原子存取（inline）＋跨檔函數宣告。
// ★ 對外 C ABI 仍只有 cabi.h；本檔只給 cabi_*.cpp 內部 include，不外洩給客戶端。
//
// 各實作檔的分工：
//   · cabi_request.cpp  → build_body（送出的請求 JSON）
//   · cabi_response.cpp → dispatch_nonstream／guard_backend_error（收回的解析與護欄）
//   · cabi_stream.cpp   → do_stream（串流 SSE 路徑）
//   · cabi.cpp          → make_request＋C ABI 出口（llm_ask/llm_cancel/llm_phase）

#include "cabi.h"
#include "http.hpp"

#include <atomic>
#include <string>

namespace cabi_impl {

// context 兩個 int 的原子存取（型別 trivial，原子性封在這裡；比照 cabi.h 的約定）。
// inline：多個 TU 都 include 本 header，inline 才不會重複定義。
inline void set_phase(llm_context_t *ctx, llm_phase_t p) {
  if (ctx)
    std::atomic_ref<int>(ctx->phase).store(static_cast<int>(p), std::memory_order_relaxed);
}
inline bool cancelled(llm_context_t *ctx) {
  return ctx && std::atomic_ref<int>(ctx->cancel).load(std::memory_order_relaxed) != 0;
}

// 連線設定（llm_client_t）→ http::Request：endpoint 預設、Bearer、逾時；body 已組好。
// def 在 cabi.cpp（transport 膠水，不需 glaze）。
http::Request make_request(const llm_client_t *c, std::string body);

// 組完整 OpenAI 請求 JSON：prompt(+media)＋response_format(schema)＋tools[]＋modalities＋取樣＋stream。
// def 在 cabi_request.cpp。
std::string build_body(const llm_client_t *c, const llm_request_t *req);

// 護欄：後端錯誤（4xx／error JSON）不得被靜默吞成空字串 → throw runtime_error。
// def 在 cabi_response.cpp。
void guard_backend_error(int status, const std::string &raw);

// 非串流：解析 choices[0].message，餵 on_text／on_tool／on_media。def 在 cabi_response.cpp。
void dispatch_nonstream(const std::string &raw, const llm_handlers_t *h);

// 串流：http::stream 逐塊 → SSE 拆框 → on_text 逐段、tool_calls 拼完整才 on_tool。
// def 在 cabi_stream.cpp。
llm_status_t do_stream(llm_context_t *ctx, const llm_client_t *c, const std::string &body,
                       const llm_handlers_t *h);

} // namespace cabi_impl
