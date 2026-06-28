// main.cpp — ac_helper 的示範程式：依託 lib 寫一支「自動會回應 --metadata」的工具。
//
// register/intercept 模型的 C++ 示範：
//   1. 純宣告 Meta（零副作用）；
//   2. main() 開頭顯式 intercept()——命中 --metadata 吐 JSON 並 exit，否則跑本體。
#include <filesystem>
#include <iostream>
#include <string>

#include "ac_helper.hpp"

int main(int argc, char** argv) {
  // ── 宣告：一個讀 stdin(text)、寫 output(binary)、stateful、冪等、可續跑、
  //          支援乾跑、峰值 256mb 的 one-shot CLI 工具。
  ac::Meta meta;
  meta.entries["stdin"]  = {ac::Entry::in,  ac::Entry::batch, ac::Entry::text};
  meta.entries["output"] = {ac::Entry::out, ac::Entry::batch, ac::Entry::binary};
  meta.stateful            = true;                         // 軸 3：寫外部
  meta.guarantee           = ac::Guarantee::idempotent;   // 軸 7
  meta.interruptible.level = ac::Interruptible::resumable; // 軸 6
  meta.allow_dry_run       = true;                         // 軸 8
  meta.resources.memory    = ac::Memory{{}, "256mb", {}};  // 軸 5：peak=256mb

  // ── 膠水：命中 `--metadata` → 序列化 JSON 到 stdout + exit(0)；否則交還控制。
  ac::intercept(argc, argv, meta);

  // ── 程式本體（沒帶 --metadata 才會到這）。
  std::cout << "core_handy demo — ac_helper version " << ac::helper_version << '\n';
  std::cout << "(帶 --metadata 跑可看九軸 JSON)\n";

  // ── 設施示範：軸 1 B 接線解析 ＋ 軸 8 dry-run。
  //    resolve 把 `--input <addr>` 解成位址字串（無 flag → 預設 "-"=stdin），
  //    這串字串可直接餵 ac::io::read_all——串起 entry 宣告 → 真實 I/O。
  const std::string in_addr = ac::cli::resolve(argc, argv, "--input", "-");
  std::cout << "input 位址解析: " << in_addr << '\n';

  const bool dry = ac::cli::is_dry_run(argc, argv);
  std::cout << "乾跑模式 (--dry-run): " << (dry ? "true" : "false") << '\n';

  // ── 軸 4 StateStore 示範：託管「跑了幾次」這個 state 值。
  //    program_name 綁定一次；用單檔形式把計數存進 state（<root>/state.json）。
  ac::state::StateStore store{"core_handy_demo"};
  std::cout << "StateStore root: " << store.root() << '\n';

  long runs = 0;
  if (store.exists(ac::state::Dir::state)) {
    // KISS：本線無 JSON parser，state 內容就是純整數字串，自己 parse。
    runs = std::stol(store.load(ac::state::Dir::state));
  }
  ++runs;

  if (dry) {
    // 乾跑：只演練、不真寫（「不碰外部狀態」是程式本體邏輯，設施只給 is_dry_run 查詢）。
    std::cout << "[dry-run] 將會把計數寫成第 " << runs << " 次（未真寫）\n";
  } else {
    store.save(ac::state::Dir::state, std::to_string(runs));
    std::cout << "本程式（在此工作目錄下）已跑第 " << runs << " 次\n";

    // 順帶示範資料夾形式：把一行成果寫進 data/ 下具名子檔。
    store.save(ac::state::Dir::data, "last_run.txt",
               "run #" + std::to_string(runs) + '\n');
    std::cout << "成果已寫入: " << store.entry_path(ac::state::Dir::data, "last_run.txt")
              << '\n';
  }

  // ── 軸 6+7 transaction 示範：write_atomic 原子寫 + 驗證無 .tmp 殘檔。
  //    寫到同目錄 <addr>.tmp → rename(tmp, addr)；rename 後 tmp 應消失（commit 完成）。
  {
    namespace fs = std::filesystem;
    const std::string addr = "core_handy_txn_demo.txt";  // CWD 下臨時驗證檔
    const std::string tmp  = addr + ".tmp";
    const std::string body = "atomic write payload — 完整或原檔不動\n";

    ac::io::write_atomic(addr, body);

    const std::string got = ac::io::read_all(addr);
    const bool content_ok = (got == body);
    const bool no_tmp     = !fs::exists(tmp);  // rename 後 tmp 必已消失
    std::cout << "[txn] write_atomic 內容完整: " << (content_ok ? "true" : "false") << '\n';
    std::cout << "[txn] rename 後無 .tmp 殘檔: " << (no_tmp ? "true" : "false") << '\n';

    // 清掉臨時驗證檔（示範用，不留垃圾）。
    fs::remove(addr);
  }

  return 0;
}
