// zhtw — 繁中翻譯薄包裝（人格單檔，C++ 移植自 zhtw）。
//
//   zhtw <要翻譯的文字...>          位置參數當輸入
//   echo ... | zhtw / zhtw < file  無位置參數時讀 stdin（管線公民）
//     → llme deepseek --temperature 0.2 --top-p 0.9 --max-tokens 4096 --system <SYSTEM> -- <前置+文字>
//
// 站在 llme 上，把 endpoint／取樣參數／人格烤死，只留使用者輸入這個變數。改下面那幾個常數即換人格。
//
// ⚠ 與 Fennel 原型的偏差：(1) 原型把 sibling 寫死 "llme"（靠 PATH）；本版預設解到同層
//   sibling <script-dir>/llme，並可用 env ZHTW_LLME 覆寫（測試設 echo 可看轉出的命令形狀）。
//   (2) 原型無參數時無條件讀 stdin；本版照 SPEC 通用約定只在 stdin 非 tty 時才讀，避免互動卡等 EOF。
#include "util.hpp"

#include <string>
#include <vector>

using handy::shquote;

// ── 烤死的設定（薄包裝的本體就這幾行，改這裡即換人格）─────────────
static const std::string ENDPOINT = "deepseek";
static const std::string SYSTEM =
  "你是專業翻譯引擎。把使用者提供的文字翻成自然、道地的繁體中文。只輸出譯文本身，不要加任何解釋、引號或前後綴。";
static const std::string PREFIX = "翻譯以下內容：\n";
static const std::vector<std::string> FLAGS = {
  "--temperature", "0.2", "--top-p", "0.9", "--max-tokens", "4096"};
// ────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
  HANDY_INIT_ARGV();
  std::vector<std::string> words(argv + 1, argv + argc);

  // 有位置參數＝所有參數併成一句；否則讀 stdin（僅非 tty 才讀，避免互動卡等 EOF）。
  std::string text;
  if (!words.empty()) {
    text = handy::join_words(words);
  } else if (handy::stdin_piped()) {
    text = handy::trim(handy::read_stdin_all());
  }
  if (text.empty()) {
    std::cerr << "用法：zhtw <要翻譯的文字...>   或   echo ... | zhtw\n";
    return 2;
  }

  std::string prompt = PREFIX + text;

  // sibling 解析：預設 <script-dir>/llme，可用 ZHTW_LLME 覆寫。
  std::string llme = handy::getenv_nonempty("ZHTW_LLME").value_or(handy::sibling("llme"));

  // 組：llme <ENDPOINT> <FLAGS...> --system <SYSTEM> -- <prompt>
  std::vector<std::string> parts = {shquote(llme), shquote(ENDPOINT)};
  for (auto& f : FLAGS) parts.push_back(shquote(f));
  parts.push_back(shquote("--system"));
  parts.push_back(shquote(SYSTEM));
  parts.push_back(shquote("--"));
  parts.push_back(shquote(prompt));

  return handy::run_system(handy::join(parts));
}
