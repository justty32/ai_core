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

#include <unistd.h>       // isatty, readlink
#include <sys/wait.h>     // WIFEXITED, WEXITSTATUS, WIFSIGNALED, WTERMSIG

namespace handy {

// 單引號包住每個參數，安全轉進 shell：' → '\''（含中文／空白／旗標）。
inline std::string shquote(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'') out += "'\\''";
    else out += c;
  }
  out += "'";
  return out;
}

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

// 本執行檔所在目錄（解 symlink，走 /proc/self/exe）。供找同層 sibling 與 configs/inbox。
inline std::string script_dir() {
  char buf[4096];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return ".";
  buf[n] = '\0';
  std::string p(buf);
  auto pos = p.rfind('/');
  return (pos == std::string::npos) ? std::string(".") : p.substr(0, pos);
}

// app 根目錄：執行檔在 <root>/bin/ 底下，configs/ 與 inbox/ 卻在 <root>/。
// 故 script_dir 若是 .../bin 就退回上一層當 root；否則 root＝script_dir（同層佈局）。
// （sibling 工具互呼仍用 script_dir——llme/mail 彼此在 bin/ 內同層。）
inline std::string app_root() {
  std::string d = script_dir();
  auto pos = d.rfind('/');
  std::string base = (pos == std::string::npos) ? d : d.substr(pos + 1);
  if (base == "bin") return (pos == std::string::npos) ? std::string(".") : d.substr(0, pos);
  return d;
}

// stdin 一次讀到 EOF。
inline std::string read_stdin_all() {
  std::ostringstream ss;
  ss << std::cin.rdbuf();
  return ss.str();
}

// stdin 非 tty（被 pipe／重導）？＝ isatty(0)==0。避免互動卡等 EOF。
inline bool stdin_piped() {
  return ::isatty(0) == 0;
}

// 執行組好的命令字串，透傳 exit code（WEXITSTATUS；被信號殺回 128+sig）。
inline int run_system(const std::string& cmd) {
  int rc = std::system(cmd.c_str());
  if (rc == -1) return 127;
  if (WIFEXITED(rc)) return WEXITSTATUS(rc);
  if (WIFSIGNALED(rc)) return 128 + WTERMSIG(rc);
  return rc;
}

// 捕捉子命令 stdout（popen；wf 的 classify 用）。
inline std::string capture(const std::string& cmd) {
  std::string out;
  FILE* p = ::popen(cmd.c_str(), "r");
  if (!p) return out;
  char buf[4096];
  size_t got;
  while ((got = std::fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, got);
  ::pclose(p);
  return out;
}

inline bool file_exists(const std::string& p) {
  std::error_code ec;
  return std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec);
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
