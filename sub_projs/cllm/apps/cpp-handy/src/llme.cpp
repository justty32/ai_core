// llme — cllm 多 endpoint dispatcher（薄轉發器，C++ 移植自 llme/_exec.fnl）。
//
//   llme <endpoint> [llm 的其餘參數...]
//     → 把 <endpoint> 翻成 <config_dir>/<endpoint>.json
//     → 轉呼 `llm --config <那個檔> [--api-key <k>] <其餘參數...>`
//
// 不 link libcllm、不重造任何東西——只是挑一個 cllm config 檔、原樣轉呼現成的 llm CLI。
//
// 環境變數：
//   LLME_LLM         覆寫要轉呼的 llm 執行檔（預設 "llm"；測試設 echo 可看轉出的參數）
//   LLME_CONFIG_DIR  覆寫 config 目錄（預設＝本 script 同層的 configs/）
//   <自動注入 api key>  呼叫 <endpoint> 時若使用者沒自帶 --api-key，依序找：
//                      ① LLME_KEY_<EP> ② <EP>_API_KEY；<EP>＝endpoint 名大寫、非英數轉 _。
#include "util.hpp"

#include <string>
#include <vector>

using handy::shquote;
using handy::getenv_nonempty;

// endpoint 名 → env 慣例的大寫識別子（非英數→_，如 deep-seek → DEEP_SEEK）。
static std::string env_stem(const std::string& s) {
  std::string out;
  for (char c : s) {
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))
      out += c;
    else if (c >= 'a' && c <= 'z')
      out += (char)(c - 32);
    else
      out += '_';
  }
  return out;
}

// 掃 config 目錄裡的 <name>.json（`_` 開頭視為模板/隱藏，不列）；排序比照 ls -1。
static std::vector<std::string> list_endpoints(const std::string& dir) {
  std::vector<std::string> out;
  std::error_code ec;
  for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) break;
    if (!e.is_regular_file(ec)) continue;
    std::string name = e.path().filename().string();
    if (name.empty() || name[0] == '_') continue;
    const std::string suf = ".json";
    if (name.size() > suf.size() && name.compare(name.size() - suf.size(), suf.size(), suf) == 0)
      out.push_back(name.substr(0, name.size() - suf.size()));
  }
  std::sort(out.begin(), out.end());
  return out;
}

static void usage(const std::string& dir) {
  std::cerr << "用法：llme <endpoint> [llm 的其餘參數...]\n"
               "  例：llme opus --stream 你好\n"
               "  <endpoint> 對應 " << dir << "/<endpoint>.json（cllm config），\n"
               "  其餘參數原樣轉給 llm CLI（--config <那個檔> 已自動帶上）。\n"
               "  環境變數：LLME_LLM 覆寫 llm 執行檔；LLME_CONFIG_DIR 覆寫 config 目錄。\n";
  auto eps = list_endpoints(dir);
  if (eps.empty()) {
    std::cerr << "  （目前 " << dir << " 下沒有可用的 endpoint config）\n";
  } else {
    std::cerr << "  可用 endpoint：";
    for (size_t i = 0; i < eps.size(); ++i) { if (i) std::cerr << ' '; std::cerr << eps[i]; }
    std::cerr << "\n";
  }
}

int main(int argc, char** argv) {
  HANDY_INIT_ARGV();
  std::vector<std::string> args(argv, argv + argc);  // args[0]=prog、args[1]=endpoint …

  std::string dir = getenv_nonempty("LLME_CONFIG_DIR").value_or(handy::app_root() + "/configs");

  bool no_ep = (argc < 2);
  std::string ep = no_ep ? std::string() : args[1];
  if (no_ep || ep == "--help" || ep == "-h") {
    usage(dir);
    return no_ep ? 2 : 0;
  }

  std::string cfg = dir + "/" + ep + ".json";
  if (!handy::file_exists(cfg)) {
    std::cerr << "llme：找不到 endpoint「" << ep << "」的 config（" << cfg << "）\n";
    usage(dir);
    return 2;
  }

  std::string llm = getenv_nonempty("LLME_LLM").value_or("llm");

  // 使用者是否已自帶 --api-key（含 --api-key=… 形式）？有就不自動注入、尊重之。
  bool user_key = false;
  for (int i = 2; i < argc; ++i) {
    std::string a = args[i];
    if (a == "--api-key" || a.rfind("--api-key=", 0) == 0) user_key = true;
  }

  std::vector<std::string> parts = {shquote(llm), "--config", shquote(cfg)};
  if (!user_key) {
    std::string stem = env_stem(ep);
    auto k = getenv_nonempty(("LLME_KEY_" + stem).c_str());
    if (!k) k = getenv_nonempty((stem + "_API_KEY").c_str());
    if (k) { parts.push_back("--api-key"); parts.push_back(shquote(*k)); }
  }
  for (int i = 2; i < argc; ++i) parts.push_back(shquote(args[i]));

  return handy::run_system(handy::join(parts));
}
