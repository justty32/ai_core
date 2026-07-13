// impl/state.hpp — 軸 4 狀態統一託管設施（StateStore；D-API Round 2）
//
// 軸 4 ⊂ 軸 1：建在 `ac::io::read_all/write_all` 的檔案分支上（見 notes/impl_1_io.md）。
// 性質：純設施（不在描述性九軸；描述面外包軸 3 `bool stateful`），程式選用託管。
//
// 標準目錄慣例（config/cache/state/data）的現成實作：路徑解析、按需建目錄、存在檢查、讀寫。
// KISS 取捨（見 notes/axis_4_state_dirs.md Round 2 決定四）：v0 進出**原始字串**、不解析 JSON
// （本線只有 emitter、無 parser）；內容格式由程式自決。
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include "io.hpp"

namespace ac::state {

// 四個標準目錄（封閉值集，同軸 7 enum 風格）。
enum class Dir : unsigned { config, cache, state, data };

// 標準目錄名。
inline std::string dir_name(Dir d) {
  switch (d) {
    case Dir::config: return "config";
    case Dir::cache:  return "cache";
    case Dir::state:  return "state";
    case Dir::data:   return "data";
  }
  return "";  // 不可達（封閉 enum 全覆蓋）；僅為消 -Wreturn-type。
}

// 託管物件：綁定一次 program_name，四目錄與讀寫掛其上。
//
// 根的解析（見 Round 2 決定二）：
//   program_name 空  → 根＝CWD          → 目錄就在 ./config …（忠於「CWD 下」）
//   program_name 有值 → 根＝./<program_name> → per-program 託管，多程式共用 CWD 不互撞
class StateStore {
 public:
  explicit StateStore(std::string program_name = "")
      : prog_(std::move(program_name)),
        root_(prog_.empty() ? std::filesystem::path{} : std::filesystem::path{prog_}) {}

  // 綁定的 program_name。
  std::string name() const { return prog_; }

  // 解析後的根（空 program_name 時為 "."）。
  std::string root() const { return root_.empty() ? "." : root_.string(); }

  // ── 路徑解析（純組字串、不碰檔案系統）──

  // 單檔形式：整個 <dir> 就是一顆 json，路徑 <root>/<dir>.json。
  std::string file_path(Dir d) const {
    return (root_ / (dir_name(d) + ".json")).string();
  }

  // 資料夾形式：<dir> 內具名子檔，路徑 <root>/<dir>/<key>。
  std::string entry_path(Dir d, const std::string& key) const {
    return (root_ / dir_name(d) / key).string();
  }

  // ── 存在檢查（不建目錄）──

  bool exists(Dir d) const {
    return std::filesystem::exists(file_path(d));
  }
  bool exists(Dir d, const std::string& key) const {
    return std::filesystem::exists(entry_path(d, key));
  }

  // ── 讀寫（建在 ac::io::read_all/write_all；KISS：原始字串）──
  // load 讀不存在路徑回空字串（沿用 ac::io::read_all 行為）；分辨「空 vs 不存在」用 exists()。

  // 單檔形式。
  std::string load(Dir d) const {
    return ac::io::read_all(file_path(d));
  }
  void save(Dir d, std::string_view data) const {
    const std::string p = file_path(d);
    ensure_parent(p);
    // 原子寫：託管狀態天生 all-or-nothing，state 檔永不因中斷半截（軸 4「data 需保護」）。
    ac::io::write_atomic(p, data);
  }

  // 資料夾形式。
  std::string load(Dir d, const std::string& key) const {
    return ac::io::read_all(entry_path(d, key));
  }
  void save(Dir d, const std::string& key, std::string_view data) const {
    const std::string p = entry_path(d, key);
    ensure_parent(p);
    // 原子寫：同上，資料夾形式具名子檔亦免費獲得軸 6 rollback + 軸 7 transactional。
    ac::io::write_atomic(p, data);
  }

 private:
  // 按需建父目錄（no-wheel-remake：用 std::filesystem，不手刻 mkdir -p）。
  static void ensure_parent(const std::string& path) {
    const auto parent = std::filesystem::path{path}.parent_path();
    if (!parent.empty()) {
      std::filesystem::create_directories(parent);
    }
  }

  std::string prog_;
  std::filesystem::path root_;
};

}  // namespace ac::state
