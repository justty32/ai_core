// main.cpp — llme：cllm 多 endpoint dispatcher（薄轉發器）
//
//   llme <endpoint> [llm 的其餘參數...]
//     → 把 <endpoint> 翻成 <config_dir>/<endpoint>.json
//     → exec `llm --config <那個檔> <其餘參數...>`
//
// 設計：不 link libcllm、不重造任何東西——只是「挑一個 cllm config 檔、
// 原樣轉呼現成的 llm CLI」。建置零依賴 cllm；runtime 只要 llm 在 PATH。
// ＝拿現成程式用薄殼包裝（handy 的方法論）；＝0708 folder-as-callable 的
// `_exec` 就是本執行體，外層 llme.sh 轉發給它。
//
// 環境變數：
//   LLME_LLM         覆寫要轉呼的 llm 執行檔（預設在 PATH 找 "llm"；測試可設 echo）
//   LLME_CONFIG_DIR  覆寫 config 目錄（預設＝本執行檔同層的 configs/）

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <limits.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

fs::path exe_dir() {
  char buf[PATH_MAX];
  const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n > 0) {
    buf[n] = '\0';
    return fs::path(buf).parent_path();
  }
  return fs::current_path();
}

fs::path config_dir() {
  if (const char* d = std::getenv("LLME_CONFIG_DIR"); d && *d) return fs::path(d);
  return exe_dir() / "configs";
}

// 掃 config 目錄裡的 <name>.json（`_` 開頭視為模板/隱藏，不列）。
std::vector<std::string> list_endpoints(const fs::path& dir) {
  std::vector<std::string> out;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(dir, ec)) {
    if (!e.is_regular_file()) continue;
    const fs::path p = e.path();
    if (p.extension() != ".json") continue;
    const std::string stem = p.stem().string();
    if (stem.empty() || stem[0] == '_') continue;
    out.push_back(stem);
  }
  std::sort(out.begin(), out.end());
  return out;
}

void print_usage(const fs::path& dir) {
  std::fprintf(stderr,
               "用法：llme <endpoint> [llm 的其餘參數...]\n"
               "  例：llme opus --stream 你好\n"
               "  <endpoint> 對應 %s/<endpoint>.json（cllm config），\n"
               "  其餘參數原樣轉給 llm CLI（--config <那個檔> 已自動帶上）。\n"
               "  環境變數：LLME_LLM 覆寫 llm 執行檔；LLME_CONFIG_DIR 覆寫 config 目錄。\n",
               dir.c_str());
  const std::vector<std::string> eps = list_endpoints(dir);
  if (eps.empty()) {
    std::fprintf(stderr, "  （目前 %s 下沒有可用的 endpoint config）\n", dir.c_str());
    return;
  }
  std::fprintf(stderr, "  可用 endpoint：");
  for (size_t i = 0; i < eps.size(); ++i)
    std::fprintf(stderr, "%s%s", i ? " " : "", eps[i].c_str());
  std::fprintf(stderr, "\n");
}

}  // namespace

int main(int argc, char** argv) {
  const fs::path dir = config_dir();

  if (argc < 2 || std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
    print_usage(dir);
    return argc < 2 ? 2 : 0;
  }

  const std::string endpoint = argv[1];
  const fs::path cfg = dir / (endpoint + ".json");
  std::error_code ec;
  if (!fs::is_regular_file(cfg, ec)) {
    std::fprintf(stderr, "llme：找不到 endpoint「%s」的 config（%s）\n",
                 endpoint.c_str(), cfg.c_str());
    print_usage(dir);
    return 2;
  }

  const char* llm = std::getenv("LLME_LLM");
  if (!llm || !*llm) llm = "llm";

  // 組轉發參數：llm --config <cfg> <argv[2..]>
  std::string llm_s = llm, flag = "--config", cfg_s = cfg.string();
  std::vector<char*> fwd{llm_s.data(), flag.data(), cfg_s.data()};
  for (int i = 2; i < argc; ++i) fwd.push_back(argv[i]);
  fwd.push_back(nullptr);

  ::execvp(llm, fwd.data());
  // execvp 只有失敗才回來
  std::fprintf(stderr, "llme：無法執行「%s」：%s（是否在 PATH？或設 LLME_LLM）\n",
               llm, std::strerror(errno));
  return 127;
}
