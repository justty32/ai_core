// main.cpp — ac_helper 的示範程式。
//
// 骨架階段：僅證明 include 與 build 鏈可運作。
// 後續會隨九軸的逐步成形，擴充成真正的設計示範。
#include <iostream>

#include "ac_helper.hpp"

int main() {
  std::cout << "core_handy demo — ac_helper version "
            << ac::helper_version << '\n';

  // 九軸定案型別的健全性示範：拼一個典型工具的 metadata。
  // 例：一個讀 stdin（text）、寫 output（binary）、stateful、冪等、可續跑、
  //     支援乾跑、峰值 256mb 的 one-shot CLI。
  ac::Meta meta;
  meta.entries["stdin"]  = {ac::Entry::in,  ac::Entry::text,   {}};
  meta.entries["output"] = {ac::Entry::out, ac::Entry::binary, {}};
  meta.stateful            = true;                         // 軸 3：寫外部
  meta.guarantee.guarantee = ac::Guarantee::idempotent;   // 軸 7
  meta.interruptible.level = ac::Interruptible::resumable; // 軸 6
  meta.allow_dry_run       = true;                         // 軸 8
  meta.resources.memory    = ac::Memory{{}, "256mb", {}};  // 軸 5：peak=256mb

  std::cout << "entries=" << meta.entries.size()
            << " stateful=" << meta.stateful
            << " guarantee=" << static_cast<unsigned>(meta.guarantee.guarantee)
            << " interruptible=" << meta.interruptible.level
            << " dry_run=" << meta.allow_dry_run << '\n';
  return 0;
}
