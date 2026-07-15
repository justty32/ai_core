// bind_bridge.cpp — bind.hpp 的 bridge_ask 實作：唯一碰 llm::Client 的地方。
//
// 「一個引擎、兩層薄綁定」的引擎那一半。s7_bind.cpp／lua_bind.cpp 只把腳本傳來的東西轉成 AskOpts、
// 呼叫本函式、再把結果塞回各自 VM——真正的 LLM 呼叫全在這裡（借編 try_3 的 llm::Client）。
//
// ★ noexcept 是刻意的：呼叫端（VM 綁定）接著可能呼 lua_error／s7_error，那是 longjmp。
//   若本函式讓 std::exception 漏出去、又在 VM 的 C 幀裡被 longjmp 跨越，即未定義行為。
//   故在這條邊界把所有例外收斂成「bool＋err 字串」，讓上層用 VM 原生的錯誤路徑回報。
//
// 取樣選項的搬運哲學：只有「腳本明確設了」（optional 有值）才覆寫，否則保留 from_env 的預設——
// 對齊 try_3 的「struct＝唯一真相源、沒設就別替後端做主」。

#include "bind.hpp"

#include <exception>

#include "llm.hpp"   // 借編 try_3/src 的 llm::Client（from_env／ask／OnDelta）

namespace vmbind {

bool bridge_ask(const AskOpts& o, std::string& out, std::string& err) noexcept {
    try {
        llm::Client c = llm::from_env();              // base：env（預設本機 LM Studio）
        if (!o.endpoint.empty()) c.endpoint = o.endpoint;  // 腳本給了才覆寫（離線 file:// fixture 靠這個）
        if (o.model)       c.model       = o.model;   // 以下取樣選項：optional 有值才覆寫
        if (o.temperature) c.temperature = o.temperature;
        if (o.top_p)       c.top_p       = o.top_p;
        if (o.max_tokens)  c.max_tokens  = o.max_tokens;
        if (o.seed)        c.seed        = o.seed;

        // 串流：把（可能為空的）on_delta 交給 ask；ask 內部對空 std::function 會略過（見 llm.cpp）。
        // 非串流：一次收完。兩種都回「完整組合後的答案」。
        out = o.stream ? c.ask(o.prompt, true, o.on_delta)
                       : c.ask(o.prompt);
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    } catch (...) {
        err = "未知錯誤（非 std::exception）";
        return false;
    }
}

}  // namespace vmbind
