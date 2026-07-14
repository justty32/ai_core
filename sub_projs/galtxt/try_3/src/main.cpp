// main.cpp — galtxt try_3 進入點：import 具名模組 demo、跑其邏輯
//
// try_3 是 galtxt 第三條實驗線：與 try_1（s7 Scheme）、try_2（C++ 內嵌 Lua）並存、互為對照。
// 這條**完全 C++**——不嵌任何腳本 VM，純原生，並用 **C++20 modules**（import demo;）當骨架示範。
//
// 跨平台：Windows 用 wmain + -municode 取寬字元命令列（中文 argv 不亂碼），
//         並 SetConsoleOutputCP(CP_UTF8) 讓主控台照 UTF-8 讀我們吐的位元組；
//         Linux/Mac 走標準 main、原生 UTF-8 argv。Windows 專屬碼以 #ifdef _WIN32 包起。
//
// 建置／除錯：見同層 README.md 與 .vscode/。CLI 直接跑：build/try3.exe [N] [名字]

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

import demo;             // ── 具名模組（提供 sum_to / greet）

static int run(const std::vector<std::string>& args) {
    // args[0]＝執行檔；args[1..]＝參數。第一個參數當作累加上限 N（預設 10），第二個當名字。
    int n = 10;
    if (args.size() >= 2) {
        n = std::atoi(args[1].c_str());
        if (n <= 0) n = 10;
    }
    std::string name = (args.size() >= 3) ? args[2] : "主人";

    std::string hello = greet(name);   // ← 來自 module demo
    long long total = sum_to(n);       // ← 來自 module demo

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
