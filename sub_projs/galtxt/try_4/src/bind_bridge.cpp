// bind_bridge.cpp — bind.hpp 的 bridge_ask 實作：唯一碰 llm::Client 的地方。
//
// 這是「一個引擎、兩層薄綁定」的引擎那一半。s7_bind.cpp／lua_bind.cpp 只把腳本傳來的字串
// 轉一轉、呼叫本函式、再把結果塞回各自 VM——真正的 LLM 呼叫全在這裡（借編 try_3 的 llm::Client）。
//
// ★ noexcept 是刻意的：呼叫端（VM 綁定）接著可能呼 lua_error／s7_error，那是 longjmp。
//   若本函式讓 std::exception 漏出去、又在 VM 的 C 幀裡被 longjmp 跨越，即未定義行為。
//   故在這條邊界上把所有例外收斂成「bool＋err 字串」，讓上層用 VM 原生的錯誤路徑回報。

#include "bind.hpp"

#include <exception>

#include "llm.hpp"   // 借編 try_3/src 的 llm::Client（from_env／ask）

namespace vmbind {

bool bridge_ask(const std::string& prompt, const std::string& endpoint,
                std::string& out, std::string& err) noexcept {
    try {
        llm::Client client = llm::from_env();     // base：env（預設本機 LM Studio）
        if (!endpoint.empty()) client.endpoint = endpoint;  // 腳本給了就覆寫（離線 file:// fixture 靠這個）
        out = client.ask(prompt);                 // 非串流：組請求→送→反射解回應→答案
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
