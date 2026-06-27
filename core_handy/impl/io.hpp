// impl/io.hpp — 軸 1 統一 I/O 設施（batch 先；D-IO Round 2）
//
// v0 範圍：位址字串 + batch 自由函式，涵蓋 std + 檔案。
// stream 群（Channel 物件 / pipe / socket / subprocess）延後到軸 2 serve / LLM 串流逼出。
// 位址慣例：`-` ＝ std（讀=stdin / 寫=stdout）；其餘 ＝ 檔案路徑。
#pragma once

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

}  // namespace ac::io
