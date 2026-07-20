#pragma once
// cli.hpp — 薄 CLI 外殼的對外介面（對齊 core-py 的 cli.py／core/src/cli.hpp）。
//
// run() 把命令列組成一次 llm::Client::ask 的發問（重用 cpp binding 的便利層聚合＋expected 錯誤）。
// 真正的活（組請求／打 HTTP／解串流）全在 binding；本殼只做「參數解析＋I/O 接線＋定退出碼」。

#include <string>
#include <vector>

namespace cli {

// CLI 主接線；回退出碼（0 成功／1 用法錯／2 請求失敗／130 取消）。
int run(const std::vector<std::string> &args);

} // namespace cli
