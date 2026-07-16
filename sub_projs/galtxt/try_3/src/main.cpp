// main.cpp — galtxt try_3 的 `llm` CLI 進入點。
//
// 只做兩件事：把命令列湊成 UTF-8 的 std::vector<std::string>、交給 cli::run。
//
// 跨平台：Windows 用 wmain + -municode 取寬字元命令列（中文 argv 不亂碼），並
//   SetConsoleOutputCP(CP_UTF8) 讓主控台照 UTF-8 讀我們吐的位元組；Linux/Mac 走標準 main、
//   原生 UTF-8 argv。（對齊舊 try_3 main 的入口慣例。）

#include <string>
#include <vector>

#include "cli.hpp"

#ifdef _WIN32
#include <windows.h>
// 寬字元 → UTF-8
static std::string w2u8(const wchar_t *w) {
  if (!w)
    return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 0)
    return {};
  std::string s(n - 1, '\0'); // n 含結尾 NUL
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  return s;
}

int wmain(int argc, wchar_t **argv) {
  SetConsoleOutputCP(CP_UTF8); // 主控台照 UTF-8 讀我們吐的位元組
  std::vector<std::string> args;
  args.reserve(argc);
  for (int i = 0; i < argc; ++i)
    args.push_back(w2u8(argv[i]));
  return cli::run(args);
}
#else
int main(int argc, char **argv) {
  std::vector<std::string> args(argv, argv + argc); // POSIX：argv 已是 UTF-8
  return cli::run(args);
}
#endif
