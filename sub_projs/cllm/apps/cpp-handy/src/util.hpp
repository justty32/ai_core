// util.hpp — handy C++ 版四工具共用的膠水工具（shquote / script-dir / exec / stdin …）。
//
// 移植自 Fennel 原型（llme/_exec.fnl、zhtw、wf、mail）；只重寫 dispatcher 膠水，
// 真正的「手」仍以 std::system / popen shell-out 給外部命令（llm cllm CLI、claude、
// 以及同層 sibling 工具）。所有進 shell 的參數一律經 shquote 轉義（吃中文／空白／旗標）。
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <io.h>        // _isatty, _fileno
#include <windows.h>   // GetModuleFileNameA, SetConsoleOutputCP, MultiByte/WideChar
#include <shellapi.h>  // CommandLineToArgvW（需連 -lshell32）
#else
#include <unistd.h>    // isatty, readlink
#include <sys/wait.h>  // WIFEXITED, WEXITSTATUS, WIFSIGNALED, WTERMSIG
#endif

namespace handy {

#ifdef _WIN32
// UTF-8 → UTF-16。Windows 的 std::system/_popen 是 narrow API，會用系統 ANSI 碼頁（繁中
// cp950）解命令字串 → UTF-8 中文變 mojibake、命令失敗。故一律轉寬字元走 _wsystem/_wpopen。
inline std::wstring utf8_to_wide(const std::string& s) {
  if (s.empty()) return {};
  int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  std::wstring w(n, L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
  return w;
}
inline std::string wide_to_utf8(const std::wstring& w) {
  if (w.empty()) return {};
  int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s(n, '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
  return s;
}
#endif

// UTF-8 字串 → std::filesystem::path。Windows 的 filesystem/fstream narrow 建構子把 string
// 當系統 ANSI 碼頁（cp950）解 → UTF-8 中文檔名 mojibake、檔案落錯位或開不了；故轉寬字元建 path。
inline std::filesystem::path fspath(const std::string& utf8) {
#ifdef _WIN32
  return std::filesystem::path(utf8_to_wide(utf8));
#else
  return std::filesystem::path(utf8);
#endif
}
// std::filesystem::path → UTF-8 顯示字串（列 inbox 檔名用；Windows 的 .string() 走 ANSI 會 mojibake）。
inline std::string path_to_utf8(const std::filesystem::path& p) {
#ifdef _WIN32
  return wide_to_utf8(p.wstring());
#else
  return p.string();
#endif
}

// 開一個到子命令的管線（capture 讀、wf inbox 寫）。Windows 走 _wpopen（寬字元，見上）。
inline FILE* popen_cmd(const std::string& cmd, const char* mode) {
#ifdef _WIN32
  wchar_t wmode[8] = {0};
  for (int i = 0; mode[i] && i < 7; ++i) wmode[i] = (wchar_t)mode[i];
  return ::_wpopen(utf8_to_wide(cmd).c_str(), wmode);
#else
  return ::popen(cmd.c_str(), mode);
#endif
}
inline int pclose_cmd(FILE* p) {
#ifdef _WIN32
  return ::_pclose(p);
#else
  return ::pclose(p);
#endif
}

#ifdef _WIN32
// Windows：run_system/capture 走 std::system/_popen＝cmd.exe。參數轉義要同時滿足
// 兩層解析：① 目標程式的 MSVCRT argv parser（雙引號＋反斜線規則）；② cmd.exe 本身
// （對 metacharacter 做 caret ^ 轉義，讓 cmd 原樣把字元交給程式）。走 cmd.exe 的好處是
// echo（內建）、claude.cmd（PATHEXT）、llm.exe、sibling .exe 全由 cmd 自然解析，免自己找。
// 參考 Daniel Colascione：先 argv-quote，再對整串 caret-escape cmd metacharacter（含引號）。
inline std::string shquote(const std::string& s) {
  // ① MSVCRT argv-quote：需引號時包雙引號，內部反斜線/雙引號照 MSVCRT 規則轉。
  std::string q;
  bool need = s.empty() || s.find_first_of(" \t\n\v\"") != std::string::npos;
  if (!need) {
    q = s;
  } else {
    q = "\"";
    for (size_t i = 0;; ++i) {
      size_t bs = 0;
      while (i < s.size() && s[i] == '\\') { ++i; ++bs; }
      if (i == s.size()) { q.append(bs * 2, '\\'); break; }
      if (s[i] == '"') { q.append(bs * 2 + 1, '\\'); q += '"'; }
      else { q.append(bs, '\\'); q += s[i]; }
    }
    q += '"';
  }
  // ② cmd.exe caret-escape：對 metacharacter（含引號）逐一前加 ^，cmd 消掉 caret 後
  //    把原字元交給程式。% 與 ! 不 caret（cmd /c 預設不做延遲展開；我方參數也無 %VAR%）。
  std::string out;
  for (char c : q) {
    if (c == '"' || c == '&' || c == '|' || c == '<' || c == '>' ||
        c == '(' || c == ')' || c == '^')
      out += '^';
    out += c;
  }
  return out;
}
#else
// POSIX：單引號包住每個參數，安全轉進 shell：' → '\''（含中文／空白／旗標）。
inline std::string shquote(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') out += "'\\''";
    else out += c;
  }
  out += "'";
  return out;
}
#endif

// 環境變數：設了且非空才回值，否則 std::nullopt。
inline std::optional<std::string> getenv_nonempty(const char* k) {
  const char* v = std::getenv(k);
  if (v && v[0] != '\0') return std::string(v);
  return std::nullopt;
}

// 去頭尾空白（Lua %s：空白／tab／換行／\v／\f／\r）。stdin 常帶結尾換行。
inline std::string trim(const std::string& s) {
  auto is_ws = [](unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
  };
  size_t b = 0, e = s.size();
  while (b < e && is_ws((unsigned char)s[b])) ++b;
  while (e > b && is_ws((unsigned char)s[e - 1])) --e;
  return s.substr(b, e - b);
}

// 本執行檔所在目錄。供找同層 sibling 與 configs/inbox。
// POSIX 走 /proc/self/exe（解 symlink）；Windows 走 GetModuleFileNameA。分隔符各自處理。
inline std::string script_dir() {
#ifdef _WIN32
  char buf[MAX_PATH];
  DWORD n = ::GetModuleFileNameA(nullptr, buf, sizeof(buf));
  if (n == 0 || n >= sizeof(buf)) return ".";
  std::string p(buf, n);
  auto pos = p.find_last_of("\\/");
  return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#else
  char buf[4096];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return ".";
  buf[n] = '\0';
  std::string p(buf);
  auto pos = p.rfind('/');
  return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
#endif
}

// app 根目錄：執行檔在 <root>/bin/ 底下，configs/ 與 inbox/ 卻在 <root>/。
// 故 script_dir 若是 .../bin 就退回上一層當 root；否則 root＝script_dir（同層佈局）。
// （sibling 工具互呼仍用 script_dir——llme/mail 彼此在 bin/ 內同層。）
inline std::string app_root() {
  std::string d = script_dir();
  auto pos = d.find_last_of("\\/");   // Windows 是反斜線、POSIX 是斜線，兩者都認
  std::string base = (pos == std::string::npos) ? d : d.substr(pos + 1);
  if (base == "bin") return (pos == std::string::npos) ? std::string(".") : d.substr(0, pos);
  return d;
}

// 同層 sibling 工具的路徑（llme/mail 互呼用）。Windows 是同層編出的 <name>.exe；
// POSIX 是同層 <name>。回絕對路徑（經 script_dir），交給 run_system 後由 shell 執行。
inline std::string sibling(const std::string& name) {
#ifdef _WIN32
  return script_dir() + "\\" + name + ".exe";
#else
  return script_dir() + "/" + name;
#endif
}

#ifdef _WIN32
// Windows：narrow argv 是系統 ANSI 碼頁（繁中 cp950），表達不了 UTF-8 中文參數。
// 從 GetCommandLineW 取 UTF-16 argv 轉 UTF-8，覆蓋 main 的 argv（配 SetConsoleOutputCP）。
// store 存字串本體、out 存指標（末端 nullptr）；兩者需活過 main 對 argv 的使用（呼叫端持有）。
inline void win_utf8_argv(int& argc, std::vector<std::string>& store, std::vector<char*>& out) {
  int n = 0;
  wchar_t** wv = ::CommandLineToArgvW(::GetCommandLineW(), &n);
  store.clear();
  out.clear();
  if (!wv) return;
  for (int i = 0; i < n; ++i) {
    int len = ::WideCharToMultiByte(CP_UTF8, 0, wv[i], -1, nullptr, 0, nullptr, nullptr);
    std::string s(len > 0 ? len - 1 : 0, '\0');
    if (len > 1) ::WideCharToMultiByte(CP_UTF8, 0, wv[i], -1, s.data(), len, nullptr, nullptr);
    store.push_back(std::move(s));
  }
  ::LocalFree(wv);
  for (auto& s : store) out.push_back(s.data());
  out.push_back(nullptr);
  argc = n;
}
#endif

// stdin 一次讀到 EOF。
inline std::string read_stdin_all() {
  std::ostringstream ss;
  ss << std::cin.rdbuf();
  return ss.str();
}

// stdin 非 tty（被 pipe／重導）？避免互動卡等 EOF。POSIX：isatty(0)；Windows：_isatty(_fileno)。
inline bool stdin_piped() {
#ifdef _WIN32
  return ::_isatty(::_fileno(stdin)) == 0;
#else
  return ::isatty(0) == 0;
#endif
}

// 執行組好的命令字串，透傳 exit code。POSIX 的 system 回值要拆 WEXITSTATUS（信號殺回 128+sig）；
// Windows 的 system 直接回程式 exit code，無 W* 巨集。
inline int run_system(const std::string& cmd) {
#ifdef _WIN32
  return ::_wsystem(utf8_to_wide(cmd).c_str());  // 寬字元 → cmd.exe 正確吃 UTF-8 中文
#else
  int rc = std::system(cmd.c_str());
  if (rc == -1) return 127;
  if (WIFEXITED(rc)) return WEXITSTATUS(rc);
  if (WIFSIGNALED(rc)) return 128 + WTERMSIG(rc);
  return rc;
#endif
}

// 捕捉子命令 stdout（wf 的 classify 用）。跨平台走 popen_cmd（Windows＝_wpopen）。
inline std::string capture(const std::string& cmd) {
  std::string out;
  FILE* p = popen_cmd(cmd, "r");
  if (!p) return out;
  char buf[4096];
  size_t got;
  while ((got = std::fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, got);
  pclose_cmd(p);
  return out;
}

inline bool file_exists(const std::string& p) {
  std::error_code ec;
  auto fp = fspath(p);  // UTF-8 → 正確 path（Windows 走寬字元，中文檔名才對）
  return std::filesystem::exists(fp, ec) && std::filesystem::is_regular_file(fp, ec);
}

// 把命令零件用空白接成一行。
inline std::string join(const std::vector<std::string>& parts) {
  std::string out;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i) out += ' ';
    out += parts[i];
  }
  return out;
}

// 把位置參數用單一空白併成一句（table.concat words " " 的等價）。
inline std::string join_words(const std::vector<std::string>& words) {
  return join(words);
}

// ASCII lower（比照 Lua string:lower 於 C locale：只動 A–Z，非 ASCII 位元組原樣）。
inline std::string ascii_lower(const std::string& s) {
  std::string out = s;
  for (char& c : out) if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
  return out;
}

// ASCII upper（比照 Lua string:upper）。
inline std::string ascii_upper(const std::string& s) {
  std::string out = s;
  for (char& c : out) if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  return out;
}

// UTF-8 安全取前 n 個「字元」（code point），不切半個字。
inline std::string utf8_head(const std::string& s, size_t n) {
  size_t cp = 0, i = 0;
  while (i < s.size()) {
    unsigned char c = (unsigned char)s[i];
    if ((c & 0xC0) != 0x80) {   // 非續接位元組＝一個 code point 的起頭
      if (cp == n) break;
      ++cp;
    }
    ++i;
  }
  return s.substr(0, i);
}

}  // namespace handy

// main() 開頭一行：Windows 下把 argv 換成 UTF-8（並設 console UTF-8 輸出）；POSIX 為 no-op。
// 各工具 main 隨即以 argv 建 std::vector<std::string>，故插在最前即可讓中文參數正確。
#ifdef _WIN32
#define HANDY_INIT_ARGV()                                                    \
  ::SetConsoleOutputCP(CP_UTF8);                                             \
  std::vector<std::string> _handy_u8;                                        \
  std::vector<char*> _handy_av;                                              \
  handy::win_utf8_argv(argc, _handy_u8, _handy_av);                          \
  argv = _handy_av.data()
#else
#define HANDY_INIT_ARGV() ((void)0)
#endif
