// impl/io.hpp — 軸 1 統一 I/O 設施（Unix「一切都是檔案」的 read/write 落地；D-IO Round 2）
//
// 對應描述面 Entry::flow（axis_1 Round 14）——作者與 hub 一律用同一對 read/write：
//   - flow=batch(0)     → read_all/write_all（整塊讀寫）。
//   - flow=streaming(1) → Reader/Writer（逐塊讀寫，已落地；範圍 std+檔案，socket/serve/fan-out 重機器延後）。
// 「transport 種類不進描述」：種類退化成位址 scheme，於本檔 open 時認；作者不分通道種類。
// 位址慣例：`-` ＝ std（讀=stdin / 寫=stdout）；其餘 ＝ 檔案路徑；`tcp://`/`shm:` 等 scheme 延後。
#pragma once

#include <cstddef>
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

// ── 串流（flow=streaming）：Reader / Writer ───────────────────────────
// 「一切都是檔案」的串流半：持有開啟的流，逐塊 read / write（同一對操作，不分通道種類）。
// 對應描述面 Entry::flow=streaming(1)；batch(0) 用上面的 read_all/write_all。
// 範圍同 batch：`-`=std、其餘=檔案。socket/tcp/FIFO scheme、SIGPIPE/背壓 robustness、
// fan-out 等重機器延後（待軸 2 serve / LLM 串流逼出，見 notes/impl_1_io.md）。
// 持有原始流故不可複製/移動（避免懸空指標）；就地構造、就地使用。

// 串流讀。`-`=stdin；其餘=檔案路徑。
class Reader {
 public:
  explicit Reader(const std::string& addr) {
    if (addr == "-") {
      in_ = &std::cin;
    } else {
      file_.open(addr, std::ios::binary);
      in_ = &file_;
    }
  }
  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  // 讀最多 max bytes（binary 用）；回傳空字串＝EOF（無更多資料）。
  std::string read(std::size_t max = 65536) {
    std::string buf(max, '\0');
    in_->read(buf.data(), static_cast<std::streamsize>(max));
    buf.resize(static_cast<std::size_t>(in_->gcount()));
    return buf;
  }
  // 讀一行（text 用，不含換行）；回傳 false＝EOF 無更多行。
  bool read_line(std::string& line) {
    return static_cast<bool>(std::getline(*in_, line));
  }
  bool eof() const { return in_->eof(); }

 private:
  std::ifstream file_;        // 非 std 時持有檔案
  std::istream* in_ = nullptr;
};

// 串流寫。`-`=stdout；其餘=檔案路徑。解構自動 flush。
class Writer {
 public:
  explicit Writer(const std::string& addr) {
    if (addr == "-") {
      out_ = &std::cout;
    } else {
      file_.open(addr, std::ios::binary);
      out_ = &file_;
    }
  }
  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;
  ~Writer() { if (out_) out_->flush(); }

  void write(std::string_view data) {
    out_->write(data.data(), static_cast<std::streamsize>(data.size()));
  }
  void flush() { out_->flush(); }

 private:
  std::ofstream file_;        // 非 std 時持有檔案
  std::ostream* out_ = nullptr;
};

}  // namespace ac::io
