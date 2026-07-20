// proc.hpp — 共用微工具：shell-out（fork/exec 收 stdout/stderr）、env、時間、字串修剪。
// 讓每個 .cpp 保持小：C++ 不引 HTTP/JSON 函式庫，全部 shell-out 給 curl/jq。
#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
namespace fs = std::filesystem;

struct Proc { std::string out, err; int code = 0; };

// 執行 argv、把 input 餵進 stdin、收 stdout/stderr 與退出碼（對齊 python subprocess.run）。
inline Proc run_proc(const std::vector<std::string>& argv, const std::string& input) {
    int in[2], out[2], er[2];
    if (pipe(in) || pipe(out) || pipe(er)) { Proc r; r.code = -1; return r; }
    pid_t pid = fork();
    if (pid == 0) {
        dup2(in[0], 0); dup2(out[1], 1); dup2(er[1], 2);
        close(in[0]); close(in[1]); close(out[0]); close(out[1]); close(er[0]); close(er[1]);
        std::vector<char*> a;
        for (auto& s : argv) a.push_back(const_cast<char*>(s.c_str()));
        a.push_back(nullptr);
        execvp(a[0], a.data());
        _exit(127);
    }
    close(in[0]); close(out[1]); close(er[1]);
    if (!input.empty()) { if (write(in[1], input.data(), input.size()) < 0) {} }
    close(in[1]);
    Proc r; char buf[8192]; ssize_t n;
    while ((n = read(out[0], buf, sizeof buf)) > 0) r.out.append(buf, n);
    while ((n = read(er[0], buf, sizeof buf)) > 0) r.err.append(buf, n);
    close(out[0]); close(er[0]);
    int st = 0; waitpid(pid, &st, 0);
    r.code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    return r;
}

inline std::string env_or(const char* k, const std::string& d) {
    const char* v = std::getenv(k);
    return (v && *v) ? std::string(v) : d;  // 空字串視同未設（對齊 python `X or default`）
}

inline fs::path exe_dir() { return fs::canonical("/proc/self/exe").parent_path(); }

inline std::string slurp_stdin() { std::ostringstream s; s << std::cin.rdbuf(); return s.str(); }

inline std::string strip(std::string s) {
    auto ws = [](unsigned char c){ return c==' '||c=='\n'||c=='\r'||c=='\t'||c=='\f'||c=='\v'; };
    size_t b = 0, e = s.size();
    while (b < e && ws(s[b])) b++;
    while (e > b && ws(s[e-1])) e--;
    return s.substr(b, e - b);
}

inline std::string utc_now() {  // ISO8601 UTC，含微秒（對齊 datetime.now(timezone.utc).isoformat()）
    struct timeval tv; gettimeofday(&tv, nullptr);
    struct tm g; gmtime_r(&tv.tv_sec, &g);
    char base[32]; strftime(base, sizeof base, "%Y-%m-%dT%H:%M:%S", &g);
    char out[64]; snprintf(out, sizeof out, "%s.%06ld+00:00", base, (long)tv.tv_usec);
    return out;
}
