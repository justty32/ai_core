// cli.hpp — galtxt try_3：由 llm::Client 欄位「反射生成」的 --flag CLI（介面）。
//
// 這是本線主張的最後一哩：try_1/try_2 從一張 schema 表生 CLI 旗標，try_3 沒有 schema 表——
// ★ 旗標名、值型別解析全從 llm::Client 的欄位反射推出（glz::reflect<Client>::keys ＋
//   glz::for_each_field）。要多一個取樣旋鈕＝在 Client 加一個欄位，CLI 旗標自動長出來、零改動。
//
// 用（cwd 需在 try_3；base 走 llm::from_env()，故預設打本機 LM Studio）：
//   try3.exe --prompt "用一句話介紹你自己"
//   try3.exe --prompt "數到五" --stream --temperature 0.7
//   try3.exe --prompt "你好" --endpoint file:///.../test/fixtures/fake/chat/completions  # 離線
//   try3.exe --help
//
// completion 印到 stdout；用法／錯誤印到 stderr。旗標名＝欄位名把底線換連字號（--max-tokens…）。

#pragma once
#include <string>
#include <vector>

namespace cli {

// 跑 CLI 模式。args＝完整命令列（args[0]＝執行檔，內部略過）。回行程結束碼。
int run(const std::vector<std::string>& args);

}  // namespace cli
