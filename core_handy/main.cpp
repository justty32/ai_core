// main.cpp — ac_helper 的示範程式：依託 lib 寫一支「自動會回應 --metadata」的工具。
//
// register/intercept 模型的 C++ 示範：
//   1. 純宣告 Meta（零副作用）；
//   2. main() 開頭顯式 intercept()——命中 --metadata 吐 JSON 並 exit，否則跑本體。
#include <iostream>

#include "ac_helper.hpp"

int main(int argc, char** argv) {
  // ── 宣告：一個讀 stdin(text)、寫 output(binary)、stateful、冪等、可續跑、
  //          支援乾跑、峰值 256mb 的 one-shot CLI 工具。
  ac::Meta meta;
  meta.entries["stdin"]  = {ac::Entry::in,  ac::Entry::text,   {}};
  meta.entries["output"] = {ac::Entry::out, ac::Entry::binary, {}};
  meta.stateful            = true;                         // 軸 3：寫外部
  meta.guarantee.guarantee = ac::Guarantee::idempotent;   // 軸 7
  meta.interruptible.level = ac::Interruptible::resumable; // 軸 6
  meta.allow_dry_run       = true;                         // 軸 8
  meta.resources.memory    = ac::Memory{{}, "256mb", {}};  // 軸 5：peak=256mb

  // ── 膠水：命中 `--metadata` → 序列化 JSON 到 stdout + exit(0)；否則交還控制。
  ac::intercept(argc, argv, meta);

  // ── 程式本體（沒帶 --metadata 才會到這）。
  std::cout << "core_handy demo — ac_helper version " << ac::helper_version << '\n';
  std::cout << "(帶 --metadata 跑可看九軸 JSON)\n";
  return 0;
}
