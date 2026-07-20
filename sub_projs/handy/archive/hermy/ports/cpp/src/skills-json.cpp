// skills-json — 掃 ../skills/*/skill.json，印出 OpenAI function-tools JSON 陣列（JSON 靠 jq）。
// folder-as-callable 的「技能發現」：每個 skills/<name>/ 需有 skill.json + run。
#include "proc.hpp"
#include <algorithm>

int main() {
    fs::path D = env_or("HERMY_SKILLS_DIR", (exe_dir().parent_path() / "skills").string());
    std::vector<std::string> objs;
    if (fs::is_directory(D)) {
        std::vector<fs::path> ds;
        for (auto& e : fs::directory_iterator(D)) ds.push_back(e.path());
        std::sort(ds.begin(), ds.end());
        const std::string filt =
            "{type:\"function\",function:{name:(.name // $dn),"
            "description:(.description // \"\"),"
            "parameters:(.parameters // {type:\"object\",properties:{}})}}";
        for (auto& d : ds) {
            std::string name = d.filename().string();
            if (!fs::is_directory(d) || name.rfind("_", 0) == 0) continue;
            fs::path meta = d / "skill.json", run = d / "run";
            if (!fs::is_regular_file(meta) || !fs::exists(run)) continue;
            Proc p = run_proc({"jq", "-c", "--arg", "dn", name, filt, meta.string()}, "");
            if (p.code != 0) {
                std::cerr << "[skills-json] " << name << " 的 skill.json 壞了：" << strip(p.err) << "\n";
                continue;
            }
            objs.push_back(strip(p.out));
        }
    }
    std::string arr = "[";
    for (size_t i = 0; i < objs.size(); i++) { if (i) arr += ","; arr += objs[i]; }
    arr += "]";
    std::cout << arr << "\n";
}
