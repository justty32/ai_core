// demo.hpp — galtxt try_3：計算/字串小示範的介面（傳統 header）。
//
// try_3 是 galtxt 第三條實驗線：與 try_1（s7 Scheme）、try_2（C++ 內嵌 Lua）並存、互為對照。
// 這條**完全 C++、純原生**——不嵌任何腳本 VM。（早期曾用 C++20 modules 當骨架，已回歸傳統 header。）

#pragma once
#include <string>

// 除錯示範目標 1：迴圈累加（中斷點下在迴圈內，觀察 acc / i 變化）
long long sum_to(int n);

// 除錯示範目標 2：字串處理（觀察 std::string 局部變數、中文位元組）
std::string greet(const std::string& name);
