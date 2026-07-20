// hermy — 編排器：把 lib/* 小原語用 CLI 串起來跑 agent 迴圈（可單獨呼叫；JSON/HTTP 全外包 jq/curl）。
// 串連：lib/skills-json（發現）→ lib/ds-chat（一次呼叫）→ lib/run-skill（執行）→ lib/mem-append（記憶）。
// messages 由本檔自管（用 jq 建構/解析）；本質映射與用法見 README.md。用法：hermy <任務...> 或 echo ... | hermy
#include "proc.hpp"
#include <fstream>

static std::string LIB;

// 呼叫 lib/<tool>：印非空 stderr、非零退出即結束（對齊 python call()）。
static std::string call(const std::string& tool, const std::vector<std::string>& args, const std::string& in) {
    std::vector<std::string> a = {(fs::path(LIB) / tool).string()};
    for (auto& x : args) a.push_back(x);
    Proc p = run_proc(a, in);
    if (!strip(p.err).empty()) std::cerr << strip(p.err) << "\n";
    if (p.code != 0) exit(p.code);
    return p.out;
}
// jq：本地 JSON 建構/解析（flags 直接放進 args，如 -c / -r / -n）。
static std::string jq(std::vector<std::string> args, const std::string& in) {
    std::vector<std::string> v = {"jq"};
    for (auto& s : args) v.push_back(s);
    return run_proc(v, in).out;
}
static void mem(const std::string& rec) { call("mem-append", {}, rec); }

int main(int argc, char** argv) {
    fs::path here = exe_dir();
    LIB = (here / "lib").string();
    std::ifstream sf((here / "prompt" / "system.md").string());
    std::stringstream ss; ss << sf.rdbuf();
    std::string SYSTEM = strip(ss.str());
    int MAX = atoi(env_or("HERMY_MAX_STEPS", "12").c_str());

    std::string task;
    for (int i = 1; i < argc; i++) { if (i > 1) task += " "; task += argv[i]; }
    task = strip(task);
    if (task.empty() && !isatty(0)) task = strip(slurp_stdin());
    if (task.empty()) { std::cerr << "用法：hermy <任務...>   或   echo ... | hermy\n"; return 2; }

    std::string tools = call("skills-json", {}, ""); if (strip(tools).empty()) tools = "[]";
    std::cerr << "[hermy] 載入 " << strip(jq({"length"}, tools)) << " 個技能\n";
    std::string messages = jq({"-c", "-n", "--arg", "sys", SYSTEM, "--arg", "task", task,
        "[{role:\"system\",content:$sys},{role:\"user\",content:$task}]"}, "");
    mem(jq({"-c", "-n", "--arg", "task", task, "{role:\"user\",content:$task}"}, ""));

    for (int step = 0; step < MAX; step++) {
        std::string req = jq({"-c", "-n", "--argjson", "messages", messages, "--argjson", "tools", tools,
            "{messages:$messages,tools:$tools}"}, "");
        std::string msg = strip(call("ds-chat", {}, req));
        messages = jq({"-c", "--argjson", "m", msg, ". + [$m]"}, messages);
        std::string calls = jq({"-c", ".tool_calls // []"}, msg);
        int n = atoi(strip(jq({"length"}, calls)).c_str());
        if (n == 0) {
            std::cout << jq({"-r", ".content // \"\""}, msg);  // jq -r 已含換行
            mem(jq({"-c", "--argjson", "m", msg, "{role:\"assistant\",content:$m.content}"}, ""));
            return 0;
        }
        for (int i = 0; i < n; i++) {
            std::string c = jq({"-c", ".[" + std::to_string(i) + "]"}, calls);
            std::string fn = strip(jq({"-r", ".function.name"}, c));
            std::string args = jq({"-r", ".function.arguments // \"{}\""}, c);
            std::string id = strip(jq({"-r", ".id"}, c));
            std::cerr << "[hermy] → 技能 " << fn << " " << strip(args) << "\n";
            std::string result = strip(call("run-skill", {fn}, args));
            if (fn == "create_skill") { tools = call("skills-json", {}, ""); if (strip(tools).empty()) tools = "[]"; }
            messages = jq({"-c", "--arg", "id", id, "--arg", "content", result,
                ". + [{role:\"tool\",tool_call_id:$id,content:$content}]"}, messages);
            mem(jq({"-c", "--arg", "fn", fn, "--arg", "r", result.substr(0, 2000),
                "{role:\"tool\",name:$fn,result:$r}"}, ""));
        }
    }
    std::cerr << "[hermy] 到達步數上限 " << MAX << "\n";
    return 0;
}
