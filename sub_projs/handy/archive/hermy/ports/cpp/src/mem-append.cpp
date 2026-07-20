// mem-append — stdin=一筆 JSON record → 加 ts → append 到 ../memory/log.ndjson（JSON 靠 jq）。
// 「記憶」的最小單元：對話後同步（append-log）。
#include "proc.hpp"
#include <fstream>

int main() {
    fs::path M = env_or("HERMY_MEMORY_DIR", (exe_dir().parent_path() / "memory").string());
    fs::create_directories(M);
    std::string raw = slurp_stdin();
    std::string ts = utc_now();
    // 正常：解析 record 後加 ts（放最後）。解析失敗：退回 {raw, ts}。
    Proc p = run_proc({"jq", "-c", "--arg", "ts", ts, ". + {ts:$ts}"}, raw.empty() ? "{}" : raw);
    std::string line;
    if (p.code != 0)
        line = run_proc({"jq", "-c", "-n", "--arg", "raw", raw, "--arg", "ts", ts, "{raw:$raw,ts:$ts}"}, "").out;
    else
        line = p.out;  // jq -c 已含換行
    std::ofstream f((M / "log.ndjson").string(), std::ios::app);
    f << line;
}
