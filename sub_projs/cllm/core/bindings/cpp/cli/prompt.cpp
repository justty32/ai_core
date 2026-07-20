// prompt.cpp — prompt.hpp 的實作：位置參數×導管 stdin 合體（對齊 cli.cpp/cli.py 的 (2) 段）。

#include "prompt.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#include "cli_internal.hpp" // kExitUsage

namespace cli::prompt {

bool build(const argv::ParsedArgs &p, std::string &prompt, int &ec) {
  bool has_dash = false;
  for (const auto &part : p.prompt_parts)
    if (part == "-") { has_dash = true; break; }

  std::string stdin_text;
  if (!isatty(fileno(stdin))) { // 只在被導管/檔案餵入時整段讀（互動終端不讀，避免卡住）
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    stdin_text = ss.str();
    while (!stdin_text.empty() && (stdin_text.back() == '\n' || stdin_text.back() == '\r'))
      stdin_text.pop_back(); // 去尾端換行，避免多餘空白進 prompt
  } else if (has_dash) {
    std::fprintf(stderr,
                 "「-」要從 stdin 讀，但 stdin 是互動終端——用導管/檔案餵入（llm-cpp --help 看用法）\n");
    ec = kExitUsage;
    return false;
  }

  for (std::size_t k = 0; k < p.prompt_parts.size(); ++k) {
    if (k) prompt += ' ';
    prompt += (p.prompt_parts[k] == "-") ? stdin_text : p.prompt_parts[k];
  }
  if (!has_dash && !stdin_text.empty()) // 沒寫「-」而兩者都有＝prompt＋空行＋stdin
    prompt = prompt.empty() ? stdin_text : prompt + "\n\n" + stdin_text;

  if (prompt.empty()) {
    std::fprintf(stderr, "缺少 prompt：給位置參數或從 stdin 餵入（llm-cpp --help 看用法）\n");
    ec = kExitUsage;
    return false;
  }
  return true;
}

} // namespace cli::prompt
