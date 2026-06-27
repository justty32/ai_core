// impl/io.hpp — 軸 1 統一 I/O 設施（batch 先；D-IO Round 2）
//
// v0 範圍：位址字串 + batch 自由函式，涵蓋 std + 檔案。
// stream 群（Channel 物件 / pipe / socket / subprocess）延後到軸 2 serve / LLM 串流逼出。
// 位址慣例：`-` ＝ std（讀=stdin / 寫=stdout）；其餘 ＝ 檔案路徑。
#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace ac::io {

// 一次讀完（batch）。"-"=stdin；其餘=檔案路徑。
inline std::string read_all(const std::string& addr) {
  std::ostringstream ss;
  if (addr == "-") {
    ss << std::cin.rdbuf();
  } else {
    std::ifstream f(addr, std::ios::binary);
    ss << f.rdbuf();
  }
  return ss.str();
}

// 一次寫完。"-"=stdout；其餘=檔案路徑。
inline void write_all(const std::string& addr, std::string_view data) {
  const auto n = static_cast<std::streamsize>(data.size());
  if (addr == "-") {
    std::cout.write(data.data(), n);
  } else {
    std::ofstream f(addr, std::ios::binary);
    f.write(data.data(), n);
  }
}

// 原子寫（軸 6 rollback + 軸 7 transactional 的共用原語；見 notes/impl_transaction.md）。
// 機制：寫到同目錄暫存檔 <addr>.tmp → std::filesystem::rename(tmp, addr)。
//   rename 在同檔系統內是原子操作 → 「全有或全無」：成功即完整覆蓋；中途崩潰/中斷
//   還沒 rename → 目標原封不動（自動 rollback），絕不出現半截/壞檔。
// 約束：
//   - tmp 必須與目標**同目錄**（addr + ".tmp"），rename 才在同 FS 內＝真原子；
//     放 /tmp 會變跨 FS copy+delete、破壞原子性。
//   - addr=="-"（串流）→ 退回 write_all：串流無原子語意，無從 rename。
//   - v0 不做 fsync（durability 留延後）：rename 原子性已保證「不出現壞檔」這個重要保證，
//     fsync 多保證的斷電持久性不是本用例（程式碼編輯/狀態託管）的重點，且需掉到 POSIX fd，
//     破壞「乾淨建在 write_all 上」。詳見 notes/impl_transaction.md Round 2 ②。
//   - 單寫者假設：tmp 用固定 ".tmp" 後綴；並發多寫者的唯一命名（pid/亂數）延後。
inline void write_atomic(const std::string& addr, std::string_view data) {
  if (addr == "-") {
    // 串流無原子語意：退回一般寫。
    write_all(addr, data);
    return;
  }
  const std::string tmp = addr + ".tmp";
  write_all(tmp, data);                              // 先全寫進同目錄暫存檔
  std::filesystem::rename(tmp, addr);               // commit：原子換名覆蓋目標
}

}  // namespace ac::io
