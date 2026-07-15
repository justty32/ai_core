// bind.hpp — galtxt try_4：把 C++ 核心綁給腳本 VM 的介面（s7／Lua 共用宣告）。
//
// try_4 的分工：C++ 核心（借編 try_3）扛重活，s7／Lua 只當薄層吃這裡開出來的 function API。
// 本檔宣告：
//   1. AskOpts —— 一次問答的所有旋鈕（prompt＋endpoint＋取樣選項＋串流回呼）。刻意用標準型別
//      （std::optional／std::function），**不含 llm.hpp**，讓兩個 VM 綁定 TU 不必扯進 llm 內部；
//      對映到 llm::Client 只發生在 bind_bridge.cpp 一處。
//   2. bridge_ask —— **唯一碰 llm::Client 的共用橋**。noexcept：把 C++ 例外收斂成 bool＋err，
//      因為 VM 的錯誤機制（lua_error／s7_error）走 longjmp，讓 C++ 例外穿越 VM 的 C 幀是 UB。
//   3/4. run_lua／run_s7 —— 各自開 VM、註冊綁定、載入並跑腳本。回 process exit code。

#pragma once
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vmbind {

// 一次問答的旋鈕。取樣選項全 optional：未設＝不覆寫 from_env 的預設（＝交後端默認，對齊 try_3 哲學）。
struct AskOpts {
    std::string prompt;                        // 必填
    std::string endpoint;                      // 空＝走 from_env 的預設（真後端）；離線給 file:// fixture
    std::optional<std::string> model;
    std::optional<float>       temperature;
    std::optional<float>       top_p;
    std::optional<int>         max_tokens;
    std::optional<int>         seed;
    bool                       stream = false; // true＝串流；逐段經 on_delta 餵出
    // 串流回呼：每段 delta 呼一次，回 true＝中止（對齊 try_3 http/llm 的極性約定）。
    // 由綁定層包成「呼叫腳本函式」的 lambda——且**內部用 VM 的保護呼叫**（lua_pcall／s7_call），
    // 讓腳本端的錯誤不會 longjmp 穿過 client.ask 的 C++ 幀。空＝不回呼（仍會串流、只是不通知）。
    std::function<bool(std::string_view)> on_delta;
};

// 共用橋（唯一碰 llm::Client 之處）：依 AskOpts 建 Client、發問。
// 回 true＝成功（答案在 out）；false＝失敗（訊息在 err）。noexcept：絕不讓例外漏到 VM 的 C 幀。
bool bridge_ask(const AskOpts& opts, std::string& out, std::string& err) noexcept;

// 各 VM 的 host：args[0]=exe、args[1]=腳本路徑、args[2..]=腳本參數。回 process exit code。
int run_lua(const std::vector<std::string>& args);
int run_s7(const std::vector<std::string>& args);

}  // namespace vmbind
