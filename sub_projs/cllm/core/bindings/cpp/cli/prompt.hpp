#pragma once
// prompt.hpp — 位置參數×導管 stdin 合體成最終 prompt（對齊 cli.cpp/cli.py 的 (2) 段）。
//
// 規則：「-」＝stdin 插入點（明指；stdin 須為導管/檔案）；沒寫「-」而兩者都有＝prompt＋空行＋stdin
// （指令在前、資料在後）；只給其一＝用那一個；互動終端一律不讀 stdin（避免卡住）。

#include <string>

#include "argv.hpp"

namespace cli::prompt {

// 組 prompt。成功回 true、填 prompt；缺 prompt／「-」遇 tty 回 false、填 ec（kExitUsage）。
bool build(const argv::ParsedArgs &p, std::string &prompt, int &ec);

} // namespace cli::prompt
