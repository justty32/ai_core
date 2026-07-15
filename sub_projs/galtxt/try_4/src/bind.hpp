// bind.hpp — galtxt try_4：把 C++ 核心綁給腳本 VM 的介面（s7／Lua 共用宣告）。
//
// try_4 的分工：C++ 核心（借編 try_3）扛重活，s7／Lua 只當薄層吃這裡開出來的 function API。
// 本檔宣告三件事：
//   1. bridge_ask —— **唯一碰 llm::Client 的共用橋**。兩個 VM 的綁定都只呼叫它，不各自重造。
//      關鍵：它 **noexcept**，把 C++ 例外收斂成「回傳 bool＋錯誤字串」——因為 VM 的錯誤機制
//      （Lua lua_error／s7 s7_error）走 longjmp，讓 C++ 例外穿越 VM 的 C 幀是未定義行為，
//      故例外必須在踏進 VM 邊界前就被吃乾淨。
//   2/3. run_lua／run_s7 —— 各自開 VM、註冊綁定、載入並跑腳本。回 process exit code。
//        兩者都吃完整 argv（args[0]=exe、args[1]=腳本、args[2..]=腳本參數），比照 try_1/try_2 host。

#pragma once
#include <string>
#include <vector>

namespace vmbind {

// 共用橋（唯一碰 llm::Client 之處）：用 from_env() 建 Client，endpoint 非空則覆寫，非串流問答。
// 回 true＝成功（答案在 out）；false＝失敗（訊息在 err）。noexcept：絕不讓例外漏到 VM 的 C 幀。
bool bridge_ask(const std::string& prompt, const std::string& endpoint,
                std::string& out, std::string& err) noexcept;

// 各 VM 的 host：args[0]=exe、args[1]=腳本路徑、args[2..]=腳本參數。回 process exit code。
int run_lua(const std::vector<std::string>& args);
int run_s7(const std::vector<std::string>& args);

}  // namespace vmbind
