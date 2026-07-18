// run-skill <name> — stdin=參數 JSON → exec ../skills/<name>/run → stdout=結果（截斷保護）。
// 「手」的最小單元：shell-out 呼叫一個 folder-as-callable 技能。
#include "proc.hpp"

int main(int argc, char** argv) {
    fs::path D = env_or("HERMY_SKILLS_DIR", (exe_dir().parent_path() / "skills").string());
    std::string name = argc > 1 ? argv[1] : "";
    fs::path run = D / name / "run";
    if (name.empty() || !fs::exists(run)) { std::cout << "[找不到技能 " << name << "]\n"; return 0; }
    std::string args = slurp_stdin();
    Proc p = run_proc({run.string()}, args);
    std::string out = strip(p.out);
    if (p.code != 0) {
        std::string e = strip(p.err);
        out = "[退出碼 " + std::to_string(p.code) + "] " + (e.empty() ? out : e);
    }
    std::cout << (out.empty() ? "（技能無輸出）" : out.substr(0, 8000)) << "\n";
}
