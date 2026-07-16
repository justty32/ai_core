// llm_entry.cpp — galtxt try_1：shell 可呼叫的統一 LLM 接口（one-shot，無 streaming）
//
// 玩具版，不套框架。形狀＝roadmap 元件1 的 one-shot 那半：prompt → completion。
//
// 用法：
//   llm_entry --prompt "你是一隻貓娘，請回答我問題"          # prompt 直接當參數
//   llm_entry --in in.txt --out out.txt                       # prompt 讀檔、結果寫檔
//   llm_entry --prompt "系統設定..." --in question.txt        # 兩者共用：--in 接在 --prompt 後面
//   echo 你好 | llm_entry                                     # 都沒給時 → 讀 stdin
//   llm_entry --prompt "寫首詩" --temp 0.1 --topp 0.4 --topk 40 --max-tokens 256
//
// 旗標（都吃「下一個 token」當值）：
//   --prompt <文字>   prompt 本體（有空白請用引號括住）
//   --in <檔>         讀檔內容，接在 --prompt 後面（中間補一個換行）
//   --out <檔>        completion 寫到檔（預設寫 stdout）
//   --model <id>      覆蓋 env 的 model
//   --temp <浮點>     temperature      ┐
//   --topp <浮點>     top_p            ├ 有給才塞進請求；沒給就不送（backend 用自己的預設）
//   --topk <整數>     top_k            │  ⚠ top_k 是非 OpenAI 標準欄位，LM Studio/本地多半支援、
//   --max-tokens <整數> max_tokens     ┘  DeepSeek 之類雲端可能不吃——按需要用。
//
// backend 走 OpenAI 相容 /v1/chat/completions，用 env 切換：
//   AI_CORE_LLM_BASE_URL  預設 http://localhost:1234/v1（LM Studio 本機預設埠）
//   AI_CORE_LLM_MODEL     預設 local-model（可被 --model 覆蓋）
//   AI_CORE_LLM_API_KEY   選填；有值才加 Authorization: Bearer
//
// HTTP 全走 curl shell-out（Windows 有 curl.exe，http/https 通吃）；JSON 回應用 simdjson 解析。
//
// 編譯（mingw64，需一起編 simdjson.cpp）：
//   g++ -std=c++20 -O2 -static -o llm_entry.exe llm_entry.cpp simdjson.cpp
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simdjson.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>  // CommandLineToArgvW
#define AC_POPEN _popen
#define AC_PCLOSE _pclose
#else
#define AC_POPEN popen
#define AC_PCLOSE pclose
#endif

namespace {

// 取命令列參數，統一成 UTF-8。
// Windows：argv 是系統 ANSI 碼頁（本機 cp950/Big5）→ 中文會亂碼；改抓寬字元命令列轉 UTF-8。
// POSIX：argv 本就 UTF-8，直接用。
std::vector<std::string> args_utf8(int argc, char** argv) {
#ifdef _WIN32
  (void)argv;
  int wargc = 0;
  wchar_t** wargv = ::CommandLineToArgvW(::GetCommandLineW(), &wargc);
  std::vector<std::string> out;
  out.reserve(wargc);
  for (int i = 0; i < wargc; ++i) {
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) ::WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), n, nullptr, nullptr);
    out.push_back(std::move(s));
  }
  ::LocalFree(wargv);
  return out;
#else
  return std::vector<std::string>(argv, argv + argc);
#endif
}

// 最小 JSON 字串轉義（組請求 body 用）。港自 core_handy meta_json.hpp::json_str。
std::string json_str(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\t': out += "\\t";  break;
      case '\r': out += "\\r";  break;
      default:   out += c;      break;
    }
  }
  out += "\"";
  return out;
}

std::string env(const char* k, const char* def) {
  const char* v = std::getenv(k);
  return std::string(v ? v : def);
}

std::string read_stream(std::istream& is) {
  std::ostringstream ss;
  ss << is.rdbuf();
  return ss.str();
}

std::string read_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) { std::cerr << "llm_entry: 讀不到檔 " << path << "\n"; std::exit(3); }
  return read_stream(f);
}

// 是否為合法 JSON number 字面（防把亂東西當數字塞進請求）。
bool is_number(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s)
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' ||
          c == '+' || c == 'e' || c == 'E'))
      return false;
  return true;
}

bool is_bool(const std::string& s) { return s == "true" || s == "false"; }

// 把 arg 用雙引號包起來給 cmd.exe（值內無 " 的前提下夠用；這些值我方掌控）。
std::string q(const std::string& s) { return "\"" + s + "\""; }

std::string run_curl(const std::string& cmd, int& exit_code) {
  FILE* pipe = AC_POPEN(cmd.c_str(), "r");
  if (!pipe) { exit_code = -1; return ""; }
  std::string out;
  char buf[65536];
  std::size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, pipe)) > 0) out.append(buf, n);
  exit_code = AC_PCLOSE(pipe);
  return out;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  ::SetConsoleOutputCP(CP_UTF8);  // 讓 UTF-8 completion 在主控台也不亂碼（寫檔不受影響）
#endif
  const std::vector<std::string> args = args_utf8(argc, argv);

  // ── 解析旗標：每個 --x 都吃下一個 token 當值。
  //    大多數旗標單值進 opt；--stop 與 --set 可重複，各自收進 vector。
  std::map<std::string, std::string> opt;
  std::vector<std::string> stops;                            // --stop（可重複 → JSON 陣列）
  std::vector<std::pair<std::string, std::string>> sets;     // --set key=rawJSON（萬用逃生口）
  for (std::size_t k = 1; k < args.size(); ++k) {
    const std::string& a = args[k];
    if (a.rfind("--", 0) != 0) {
      std::cerr << "llm_entry: 看不懂的參數「" << a << "」（要 --prompt/--in/--out/--temp…）\n";
      return 2;
    }
    const std::string key = a.substr(2);
    if (k + 1 >= args.size()) { std::cerr << "llm_entry: --" << key << " 少了值\n"; return 2; }
    const std::string val = args[++k];
    if (key == "stop") {
      stops.push_back(val);
    } else if (key == "set") {
      const std::size_t eq = val.find('=');
      if (eq == std::string::npos || eq == 0) {
        std::cerr << "llm_entry: --set 需要 key=value（value 是原始 JSON），得到「" << val << "」\n";
        return 2;
      }
      sets.emplace_back(val.substr(0, eq), val.substr(eq + 1));
    } else {
      opt[key] = val;
    }
  }

  // ── 擋未知旗標（打錯字早死，不要靜靜把它當單值旗標吞掉）。
  static const std::set<std::string> known = {
      "prompt", "in", "out", "model", "system",              // 內容/路由
      "temp", "topp", "topk", "max-tokens",                  // 常用取樣
      "presence-penalty", "frequency-penalty", "seed", "n",  // 其餘數字參數
      "logprobs", "stream"};                                 // 布林
  for (const auto& [k, v] : opt) {
    (void)v;
    if (!known.count(k)) {
      std::cerr << "llm_entry: 不認得的旗標 --" << k
                << "（要設 API 其他欄位用 --set " << k << "=<JSON>）\n";
      return 2;
    }
  }

  // ── 組 prompt：--prompt 本體 ＋（--in 檔內容接在後面，中間補換行）；都沒給就讀 stdin。
  std::string prompt;
  if (opt.count("prompt")) prompt = opt["prompt"];
  if (opt.count("in")) {
    const std::string body = read_file(opt["in"]);
    if (!prompt.empty() && !body.empty()) prompt += "\n";
    prompt += body;
  }
  if (!opt.count("prompt") && !opt.count("in")) prompt = read_stream(std::cin);
  if (prompt.empty()) {
    std::cerr << "llm_entry: 沒有 prompt（給 --prompt / --in，或從 stdin 餵）\n";
    return 2;
  }

  const std::string base  = env("AI_CORE_LLM_BASE_URL", "http://localhost:1234/v1");
  const std::string model = opt.count("model") ? opt["model"]
                                               : env("AI_CORE_LLM_MODEL", "local-model");
  const std::string key   = env("AI_CORE_LLM_API_KEY", "");

  // ── 各項可設參數：有給才加（沒給讓 backend 用自己的預設）。旗標名→OpenAI 欄位。
  std::string params;

  // 數字欄位。
  const std::pair<const char*, const char*> num_opts[] = {
      {"temp", "temperature"},           {"topp", "top_p"},
      {"topk", "top_k"},                 {"max-tokens", "max_tokens"},
      {"presence-penalty", "presence_penalty"},
      {"frequency-penalty", "frequency_penalty"},
      {"seed", "seed"},                  {"n", "n"}};
  for (const auto& [flag, field] : num_opts) {
    const auto it = opt.find(flag);
    if (it == opt.end()) continue;
    if (!is_number(it->second)) {
      std::cerr << "llm_entry: --" << flag << " 需要數字，得到「" << it->second << "」\n";
      return 2;
    }
    params += ",\"" + std::string(field) + "\":" + it->second;  // 數字字面直接內嵌
  }

  // 布林欄位。
  const std::pair<const char*, const char*> bool_opts[] = {{"logprobs", "logprobs"}};
  for (const auto& [flag, field] : bool_opts) {
    const auto it = opt.find(flag);
    if (it == opt.end()) continue;
    if (!is_bool(it->second)) {
      std::cerr << "llm_entry: --" << flag << " 需要 true/false，得到「" << it->second << "」\n";
      return 2;
    }
    params += ",\"" + std::string(field) + "\":" + it->second;
  }

  // stop：可重複 → JSON 字串陣列。
  if (!stops.empty()) {
    params += ",\"stop\":[";
    for (std::size_t i = 0; i < stops.size(); ++i) {
      if (i) params += ",";
      params += json_str(stops[i]);
    }
    params += "]";
  }

  // 萬用逃生口 --set：value 原封當 JSON 內嵌（未來 API 加新欄位免改程式）。
  for (const auto& [k, v] : sets) params += ",\"" + k + "\":" + v;

  // streaming：旗標收下，但功能先擱著——一律以 stream:false 送出（設 true 給提示）。
  if (opt.count("stream")) {
    if (!is_bool(opt["stream"])) {
      std::cerr << "llm_entry: --stream 需要 true/false，得到「" << opt["stream"] << "」\n";
      return 2;
    }
    if (opt["stream"] == "true")
      std::cerr << "llm_entry: 注意——streaming 尚未實作，本次仍以 stream:false 送出\n";
  }

  // ── messages：有 --system 就前置一則 system role。
  std::string messages = "[";
  if (opt.count("system"))
    messages += "{\"role\":\"system\",\"content\":" + json_str(opt["system"]) + "},";
  messages += "{\"role\":\"user\",\"content\":" + json_str(prompt) + "}]";

  // ── 組請求 body，寫進暫存檔（避免把 prompt 塞進命令列的引號地獄）。
  const std::string request = "{\"model\":" + json_str(model) + ",\"messages\":" + messages +
                              ",\"stream\":false" + params + "}";

  const std::filesystem::path req =
      std::filesystem::temp_directory_path() / "galtxt_llm_req.json";
  {
    std::ofstream f(req, std::ios::binary);
    if (!f) { std::cerr << "llm_entry: 無法寫暫存檔 " << req.string() << "\n"; return 3; }
    f << request;
  }

  // ── 組 curl 命令（--data-binary 隱含 POST；2>&1 讓連線錯誤也被抓到）。
  std::string cmd = "curl -sS " + q(base + "/chat/completions") +
                    " -H " + q("Content-Type: application/json");
  if (!key.empty()) cmd += " -H " + q("Authorization: Bearer " + key);
  cmd += " --data-binary " + q("@" + req.string()) + " 2>&1";

  int code = 0;
  std::string resp = run_curl(cmd, code);
  std::error_code ec;
  std::filesystem::remove(req, ec);

  // ── 用 simdjson 解析 choices[0].message.content。
  simdjson::dom::parser parser;
  simdjson::dom::element doc;
  std::string_view content;
  const auto perr = parser.parse(resp).get(doc);
  const auto cerr = perr ? perr
                         : doc["choices"].at(0)["message"]["content"].get(content);
  if (cerr) {
    std::cerr << "llm_entry: 回應無 choices[0].message.content（curl exit " << code
              << "；" << simdjson::error_message(cerr) << "）。原始回應：\n"
              << resp << "\n";
    return 1;
  }

  // ── completion 輸出：--out 寫檔（原樣、無尾換行）；否則 stdout（補換行好讀）。
  if (opt.count("out")) {
    std::ofstream f(opt["out"], std::ios::binary);
    if (!f) { std::cerr << "llm_entry: 無法寫 --out 檔 " << opt["out"] << "\n"; return 3; }
    f << content;
  } else {
    std::cout << content << "\n";
  }

  // token 用量出 stderr（輕量計量，仿 core_handy [meter]）。
  std::int64_t toks = 0;
  if (!doc["usage"]["total_tokens"].get(toks) && toks > 0)
    std::cerr << "[meter] total_tokens=" << toks << "\n";
  return 0;
}
