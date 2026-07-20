// mail — inbox 協議的可執行版（C++ 移植自 mail）：把「太複雜、不想當場等」的任務寫成一封信
//        投進 inbox/，之後由 `mail run` 逐封 spawn claude code 這類成熟 agent 去做。
//
//   mail send <任務...>        寫一封信到 inbox/<slug>.md，印出路徑，立即返回
//     cat ctx | mail send <任務>   pipe 當上下文寫進信
//   mail list                  列出 inbox/ 頂層待處理的信（done/ 不列）
//   mail run [slug]            處理：對每封（或指定 slug）spawn claude -p 跑，成功 mv 到 done/
//   mail run --dry             只列會處理哪些、不真跑
//
// 環境變數：INBOX_DIR（投遞區，預設同層 inbox/）、WF_CLAUDE（claude，與 wf 共用）、
//   WF_PERMISSION（claude --permission-mode，與 wf 共用）。
#include "util.hpp"

#include <string>
#include <vector>
#include <fstream>

using handy::shquote;
using handy::getenv_nonempty;

// 掃某目錄頂層的 <name>.md（不遞迴、done/ 不含）；排序比照 ls -1。
static std::vector<std::string> list_md(const std::string& dir) {
  std::vector<std::string> out;
  std::error_code ec;
  for (auto& e : std::filesystem::directory_iterator(handy::fspath(dir), ec)) {
    if (ec) break;
    if (!e.is_regular_file(ec)) continue;
    std::string name = handy::path_to_utf8(e.path().filename());  // Windows 中文檔名要 UTF-8 還原
    const std::string suf = ".md";
    if (name.size() > suf.size() && name.compare(name.size() - suf.size(), suf.size(), suf) == 0)
      out.push_back(name.substr(0, name.size() - suf.size()));
  }
  std::sort(out.begin(), out.end());
  return out;
}

// 連續 ASCII 空白 → 單一 "-"（Lua gsub "%s+" "-"）。
static std::string ws_to_dash(const std::string& s) {
  auto is_ws = [](unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
  };
  std::string out;
  bool in_ws = false;
  for (unsigned char c : s) {
    if (is_ws(c)) { if (!in_ws) { out += '-'; in_ws = true; } }
    else { out += (char)c; in_ws = false; }
  }
  return out;
}

// 去掉檔名不安全字元 / \ : * ? " ' < > |（Lua gsub "[/\\:*?\"'<>|]" ""）。
static std::string strip_unsafe(const std::string& s) {
  static const std::string bad = "/\\:*?\"'<>|";
  std::string out;
  for (char c : s) if (bad.find(c) == std::string::npos) out += c;
  return out;
}

// 連續 "-" 併一個（Lua gsub "%-+" "-"）。
static std::string collapse_dash(const std::string& s) {
  std::string out;
  bool prev = false;
  for (char c : s) {
    if (c == '-') { if (!prev) { out += '-'; prev = true; } }
    else { out += c; prev = false; }
  }
  return out;
}

// slug：任務前段轉 kebab、去不安全字元、限 16 字（UTF-8 安全）；空則 "task"。
static std::string slugify(const std::string& task) {
  std::string t = handy::trim(task);
  t = ws_to_dash(t);
  t = strip_unsafe(t);
  t = collapse_dash(t);
  if (!t.empty() && t.front() == '-') t.erase(t.begin());      // ^%-
  t = handy::utf8_head(t, 16);
  if (!t.empty() && t.back() == '-') t.pop_back();             // %-$
  return t.empty() ? std::string("task") : t;
}

static std::string read_file(const std::string& p) {
  std::ifstream f(handy::fspath(p), std::ios::binary);
  if (!f) return "";
  std::ostringstream ss; ss << f.rdbuf();
  return ss.str();
}

static void write_file(const std::string& p, const std::string& body) {
  std::ofstream f(handy::fspath(p), std::ios::binary);
  f << body;
}

int main(int argc, char** argv) {
  HANDY_INIT_ARGV();
  std::vector<std::string> args(argv, argv + argc);

  std::string inbox_dir = getenv_nonempty("INBOX_DIR").value_or(handy::app_root() + "/inbox");
  std::string done_dir = inbox_dir + "/done";

  auto ensure_dirs = [&]() {
    std::error_code ec;
    std::filesystem::create_directories(handy::fspath(done_dir), ec);
  };

  auto unique_path = [&](const std::string& slug) -> std::string {
    std::string p = inbox_dir + "/" + slug + ".md";
    int n = 2;
    while (handy::file_exists(p)) { p = inbox_dir + "/" + slug + "-" + std::to_string(n) + ".md"; ++n; }
    return p;
  };

  std::string sub = (argc >= 2) ? args[1] : std::string();
  bool no_sub = (argc < 2);

  // ── send ────────────────────────────────────────────────────
  if (sub == "send") {
    std::vector<std::string> words(args.begin() + 2, args.end());
    std::string task = handy::join_words(words);
    bool piped = handy::stdin_piped();
    std::string ctx = piped ? handy::trim(handy::read_stdin_all()) : std::string();
    bool has_ctx = !ctx.empty();
    if (task.empty() && !has_ctx) {
      std::cerr << "用法：mail send <任務...>（或 … | mail send <任務>）\n";
      return 2;
    }
    std::string real_task = !task.empty() ? task : ctx;
    std::string slug = slugify(real_task);
    std::string path = unique_path(slug);
    std::string ctx_field = (has_ctx && !task.empty()) ? ctx : std::string("（無）");
    std::string body = "# " + slug + "\n\n## 任務\n" + real_task + "\n\n## 上下文\n" + ctx_field + "\n\n"
                       "---\n（此信由 handy inbox 投遞；請完整完成信中的任務。處理者會於完成後將本信歸檔到 done/。）\n";
    ensure_dirs();
    write_file(path, body);
    std::cout << "已投遞：" << path << "\n";
    return 0;
  }

  // ── list / ls ───────────────────────────────────────────────
  if (sub == "list" || sub == "ls") {
    auto pending = list_md(inbox_dir);
    if (pending.empty()) {
      std::cout << "（inbox 空：沒有待處理的信）\n";
    } else {
      std::cout << "待處理的信（" << pending.size() << "）：\n";
      for (auto& s : pending) std::cout << "  " << s << "\n";
    }
    return 0;
  }

  // ── run [slug|--dry] ────────────────────────────────────────
  if (sub == "run") {
    std::string claude = getenv_nonempty("WF_CLAUDE").value_or("claude");
    std::string permission = getenv_nonempty("WF_PERMISSION").value_or("acceptEdits");
    std::string arg2 = (argc >= 3) ? args[2] : std::string();
    bool have_arg2 = (argc >= 3);
    bool dry = (arg2 == "--dry");
    bool has_only = have_arg2 && !dry;
    std::vector<std::string> pending = has_only ? std::vector<std::string>{arg2} : list_md(inbox_dir);

    if (pending.empty()) { std::cout << "（inbox 空：無事可做）\n"; return 0; }
    ensure_dirs();
    int failed = 0;
    for (auto& slug : pending) {
      std::string path = inbox_dir + "/" + slug + ".md";
      if (!handy::file_exists(path)) {
        std::cerr << "跳過（找不到）：" << slug << "\n";
        ++failed;
        continue;
      }
      std::string letter = read_file(path);
      std::string prompt = "以下是一封待處理的任務信，請完整完成信中的任務：\n\n" + letter;
      std::cout << "\n=== 處理：" << slug << " ===\n";
      if (dry) {
        std::cout << "  [--dry] 略過不跑\n";
        continue;
      }
      std::string cmd = handy::join({
        shquote(claude), "-p", "--permission-mode", shquote(permission),
        "--", shquote(prompt)});
      int code = handy::run_system(cmd);
      if (code == 0) {
        std::error_code ec;
        std::filesystem::rename(handy::fspath(path), handy::fspath(done_dir + "/" + slug + ".md"), ec);
        std::cout << "  ✓ 完成，歸檔 → done/" << slug << ".md\n";
      } else {
        std::cerr << "  ✗ claude 退出碼 " << code << "，保留在 inbox（未歸檔）\n";
        ++failed;
      }
    }
    return failed > 0 ? 1 : 0;
  }

  // ── 未知子命令 ──────────────────────────────────────────────
  std::cerr << "用法：mail <send|list|run>\n"
               "  send <任務...>   投遞一封信到 inbox/（… | mail send <任務> 可帶上下文）\n"
               "  list             列出待處理的信\n"
               "  run [slug|--dry] 逐封 spawn claude 處理，成功歸檔到 done/\n";
  return no_sub ? 2 : 0;
}
