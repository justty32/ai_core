// main.cpp — galtxt try_3：純 C++ 專案骨架（CMake + MinGW + VSCode/gdb 除錯）
//
// try_3 是 galtxt 第三條實驗線：與 try_1（s7 Scheme）、try_2（C++ 內嵌 Lua）並存、互為對照。
// 這條**完全 C++**——不嵌任何腳本 VM，純原生。本檔目前只是可建置、可除錯的最小骨架，
// 提供幾個好下中斷點的目標（迴圈累加、字串處理），讓 VSCode + gdb 的除錯環境先跑起來。
//
// 跨平台：Windows 用 wmain + -municode 取寬字元命令列（中文 argv 不亂碼），
//         並 SetConsoleOutputCP(CP_UTF8) 讓主控台照 UTF-8 讀我們吐的位元組；
//         Linux/Mac 走標準 main、原生 UTF-8 argv。所有 Windows 專屬碼以 #ifdef _WIN32 包起。
//
// 建置／除錯：見同層 README.md 與 .vscode/。CLI 直接跑：build/try3.exe [N]

#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
// 寬字元 → UTF-8（對齊 try_2 host.cpp 的 w2u8）
static std::string w2u8(const wchar_t* w) {
    if (!w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(n - 1, '\0');  // n 含結尾 NUL
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
#endif

// ── 除錯示範目標 1：迴圈累加（在迴圈內下中斷點，觀察 acc / i 變化）
static long long sum_to(int n) {
    long long acc = 0;
    for (int i = 1; i <= n; ++i) {
        acc += i;
    }
    return acc;
}

// ── 除錯示範目標 2：字串處理（觀察 std::string 局部變數、中文位元組）
static std::string greet(const std::string& name) {
    std::string msg = "你好，" + name + "！這是 galtxt try_3（純 C++）。";
    return msg;
}

static int run(const std::vector<std::string>& args) {
    // args[0]＝執行檔；args[1..]＝參數。第一個參數當作累加上限 N（預設 10）。
    int n = 10;
    if (args.size() >= 2) {
        n = std::atoi(args[1].c_str());
        if (n <= 0) n = 10;
    }
    std::string name = (args.size() >= 3) ? args[2] : "主人";

    std::string hello = greet(name);
    long long total = sum_to(n);

    std::printf("%s\n", hello.c_str());
    std::printf("1..%d 的和 = %lld\n", n, total);
    return 0;
}

#ifdef _WIN32
int wmain(int argc, wchar_t** wargv) {
    // 主控台改用 UTF-8：我們吐 UTF-8 位元組，終端碼頁若是 950/936 會被讀成亂碼。
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) args.push_back(w2u8(wargv[i]));
    return run(args);
}
#else
int main(int argc, char** argv) {
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
    return run(args);
}
#endif
