// wf — 兩層任務派發器（C++ 移植自 wf）：用 llme（DeepSeek）當路由腦判斷任務「要不要動手」，
//      只需腦的走 llme、要動手的走 claude code（headless agent），太複雜的用 -i 投 inbox。
//
//   wf 幫我把 foo.py 的錯誤處理補上     auto：分類→多半判 AGENT→claude 動手
//   wf 用繁中解釋什麼是 mmap            auto：判 BRAIN→llme 直接答
//   cat foo.py | wf 幫我改、加型別註記    pipe＝附給任務的上下文
//   wf -b <任務>                        強制 BRAIN（走 llme）
//   wf -a <任務>                        強制 AGENT（走 claude）
//   wf -i <任務>                        非同步投遞 inbox（轉 mail send）
//   wf < TASK.md                        無參數＝整個 stdin 當任務
//
// 決策順序＝強制旗標 ＞ 啟發式關鍵詞（免 LLM）＞ LLM 分類。
//
// 環境變數：WF_ENDPOINT(deepseek)、WF_LLME(sibling llme)、WF_CLAUDE(claude)、
//   WF_PERMISSION(acceptEdits)、WF_MODEL、WF_EFFORT、WF_SYSTEM(預設繁中溝通；設空字串關掉)。
#include "util.hpp"

#include <string>
#include <vector>

using handy::shquote;
using handy::getenv_nonempty;

// 高精度關鍵詞（寧可漏判交給 LLM，也別誤判）；比對 lower 後的字串。
static const std::vector<std::string> AGENT_PATS = {  // 需要動手／落檔／執行
  "改", "修改", "修復", "修正", "修好", "改好", "弄好", "搞定", "重構", "重寫", "改寫", "改成",
  "新增", "加上", "增加", "刪除", "刪掉", "移除", "替換", "取代", "實作", "實現", "執行", "跑一下",
  "編譯", "打包", "測試", "部署", "安裝", "建立", "建置", "初始化", "清理", "重命名", "格式化",
  "refactor", "fix", "edit", "implement", "rename", "deploy", "install", "commit", "build"};
static const std::vector<std::string> BRAIN_PATS = {  // 只需生成／回答（不落檔）
  "解釋", "說明", "什麼是", "甚麼是", "何謂", "為什麼", "為何", "翻譯", "總結", "摘要", "概述",
  "定義", "舉例", "比較", "介紹", "explain", "translate", "summarize", "summarise", "define", "what is", "why"};

static bool any_match(const std::string& low, const std::vector<std::string>& pats) {
  for (auto& p : pats)
    if (low.find(p) != std::string::npos) return true;
  return false;
}

// 回 "agent" / "brain" / ""（模稜兩可＝空字串→交給 LLM）。
static std::string heuristic_route(const std::string& text) {
  std::string low = handy::ascii_lower(text);
  bool a = any_match(low, AGENT_PATS);
  bool b = any_match(low, BRAIN_PATS);
  if (a && !b) return "agent";
  if (b && !a) return "brain";
  return "";
}

int main(int argc, char** argv) {
  HANDY_INIT_ARGV();
  std::vector<std::string> args(argv, argv + argc);

  // ── 解析前置旗標（-b/-a/-i、-- 停止、-h）＋收任務詞 ──────────
  std::string force;  // ""＝auto；"brain"／"agent"／"inbox"
  bool stop = false;
  std::vector<std::string> words;
  for (int i = 1; i < argc; ++i) {
    std::string a = args[i];
    if (stop) { words.push_back(a); continue; }
    if (a == "-b" || a == "--brain") force = "brain";
    else if (a == "-a" || a == "--agent") force = "agent";
    else if (a == "-i" || a == "--inbox") force = "inbox";
    else if (a == "--") stop = true;
    else if (a == "-h" || a == "--help") {
      std::cerr << "用法：wf [-b|-a] <任務...>；或 … | wf <任務>；或 wf < TASK.md\n"
                   "  -b 強制走 llme（只需腦）；-a 強制走 claude（要動手）；預設 auto 分類。\n";
      return 0;
    } else words.push_back(a);
  }

  // ── 取輸入：argv＝任務指令；pipe＝上下文 ─────────────────────
  std::string task = handy::join_words(words);
  bool piped = handy::stdin_piped();
  std::string ctx = piped ? handy::trim(handy::read_stdin_all()) : std::string();
  bool has_ctx = !ctx.empty();

  bool have_prompt = false;
  std::string prompt;
  if (!task.empty()) {
    have_prompt = true;
    prompt = has_ctx ? (task + "\n\n--- 以下為透過管線傳入的上下文 ---\n" + ctx) : task;
  } else if (has_ctx) {
    have_prompt = true;
    prompt = ctx;
  }

  if (!have_prompt) {
    std::cerr << "用法：wf [-b|-a] <任務...>            例：wf 幫我把 foo.py 的錯誤處理補上\n"
                 "      cat foo.py | wf 幫我改          （pipe 當上下文附給任務）\n"
                 "      wf < TASK.md                    （無參數＝整個 stdin 當任務）\n"
                 "  -b 強制 llme（只需腦）／-a 強制 claude（要動手）；預設 auto 由 llme 分類。\n";
    return 2;
  }

  std::string endpoint = getenv_nonempty("WF_ENDPOINT").value_or("deepseek");
  std::string llme = getenv_nonempty("WF_LLME").value_or(handy::sibling("llme"));
  std::string claude = getenv_nonempty("WF_CLAUDE").value_or("claude");
  // WF_SYSTEM：未設（NULL）→ 預設繁中；已設（含空字串）→ 用其值（空字串關掉）。
  const char* sys_env = std::getenv("WF_SYSTEM");
  std::string system = sys_env ? std::string(sys_env) : std::string("與使用者溝通時用繁體中文。");

  const std::string CLASSIFY_SYS =
    "你是任務路由器。判斷使用者的任務**是否需要實際動手**（讀寫檔案、執行指令、"
    "跨多檔修改、跑測試等 agent 能力）。需要動手→回 AGENT；只需生成文字／回答／"
    "翻譯／摘要／寫一段內容（不落檔）→回 BRAIN。只回一個詞：AGENT 或 BRAIN。";

  // LLM 分類：capture llme <ep> --temperature 0 --max-tokens 16 --system <SYS> -- <text>。
  auto classify = [&](const std::string& text) -> std::string {
    std::vector<std::string> parts = {
      shquote(llme), shquote(endpoint),
      "--temperature", "0", "--max-tokens", "16",
      "--system", shquote(CLASSIFY_SYS),
      "--", shquote(text)};
    std::string out = handy::ascii_upper(handy::trim(handy::capture(handy::join(parts))));
    if (out.find("BRAIN") != std::string::npos) return "brain";
    if (out.find("AGENT") != std::string::npos) return "agent";
    return "agent";  // 不清楚/失敗→偏向能完成動手任務，較安全
  };

  // 決策順序：強制旗標 ＞ 啟發式（免 LLM）＞ LLM 分類。
  std::string heur = force.empty() ? heuristic_route(prompt) : std::string();
  std::string route = !force.empty() ? force : (!heur.empty() ? heur : classify(prompt));
  std::string how = !force.empty() ? "強制"
                    : (!heur.empty() ? "啟發式・免 LLM" : ("llme 分類，endpoint=" + endpoint));
  std::cerr << "[wf] 路由：" << route << "（" << how << "）\n";

  // inbox 模式：非同步投遞，轉呼同層 mail send（task 與 ctx 分開傳，slug 才乾淨）。
  if (route == "inbox") {
    std::string mail = handy::sibling("mail");
    std::string send_task = !task.empty() ? task : ctx;
    std::string base = handy::join({shquote(mail), "send", shquote(send_task)});
    if (has_ctx && !task.empty()) {
      // ctx 經 stdin 餵給 mail send。POSIX 用 `printf %s <ctx> | mail`；Windows 無 printf，
      // 改以 _popen("w") 開 mail 的 stdin 寫入（run_system 走 cmd.exe，路徑一致）。
#ifdef _WIN32
      FILE* p = handy::popen_cmd(base, "w");
      if (!p) return 127;
      std::fwrite(ctx.data(), 1, ctx.size(), p);
      return handy::pclose_cmd(p);  // _pclose 直接回子程序 exit code
#else
      return handy::run_system("printf %s " + shquote(ctx) + " | " + base);
#endif
    }
    return handy::run_system(base);
  }

  // ── 執行（brain / agent 當場跑）──────────────────────────────
  std::vector<std::string> parts;
  if (route == "brain") {
    parts = {shquote(llme), shquote(endpoint), "--stream"};
    if (!system.empty()) { parts.push_back("--system"); parts.push_back(shquote(system)); }
    parts.push_back("--"); parts.push_back(shquote(prompt));
  } else {
    std::string permission = getenv_nonempty("WF_PERMISSION").value_or("acceptEdits");
    parts = {shquote(claude), "-p", "--permission-mode", shquote(permission)};
    if (!system.empty()) { parts.push_back("--append-system-prompt"); parts.push_back(shquote(system)); }
    if (auto m = getenv_nonempty("WF_MODEL"))  { parts.push_back("--model");  parts.push_back(shquote(*m)); }
    if (auto e = getenv_nonempty("WF_EFFORT")) { parts.push_back("--effort"); parts.push_back(shquote(*e)); }
    parts.push_back("--"); parts.push_back(shquote(prompt));
  }

  return handy::run_system(handy::join(parts));
}
