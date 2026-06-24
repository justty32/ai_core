// main.cpp — ac_helper 的示範程式。
//
// 骨架階段：僅證明 include 與 build 鏈可運作。
// 後續會隨九軸的逐步成形，擴充成真正的設計示範。
#include <iostream>

#include "ac_helper.hpp"

int main() {
  std::cout << "core_handy demo — ac_helper version "
            << ac::helper_version << '\n';
  return 0;
}
