// demo.cpp — demo.hpp 的實作。

#include "demo.hpp"

long long sum_to(int n) {
    long long acc = 0;
    for (int i = 1; i <= n; ++i) {
        acc += i;
    }
    return acc;
}

std::string greet(const std::string& name) {
    std::string msg = "你好，" + name + "！這是 galtxt try_3（純 C++、原生）。";
    return msg;
}
