// demo.cppm — galtxt try_3：C++20 named module 示範
//
// 把「邏輯」（sum_to 累加、greet 組字串）放進具名模組 demo；main.cpp 以 import demo; 取用。
// 具名模組的傳統標頭要放在「全域模組片段」（module; 之後、export module 之前）裡 #include，
// 模組本體只 export 我們要對外公開的東西。編譯產物：demo.gcm（BMI）+ demo.cppm.obj。

module;                 // ── 全域模組片段：傳統 header 在這裡 include
#include <string>

export module demo;     // ── 具名模組 demo 的主介面單元

// 除錯示範目標 1：迴圈累加（中斷點下在迴圈內，觀察 acc / i 變化）
export long long sum_to(int n) {
    long long acc = 0;
    for (int i = 1; i <= n; ++i) {
        acc += i;
    }
    return acc;
}

// 除錯示範目標 2：字串處理（觀察 std::string 局部變數、中文位元組）
export std::string greet(const std::string& name) {
    std::string msg = "你好，" + name + "！這是 galtxt try_3（純 C++／C++20 modules）。";
    return msg;
}
