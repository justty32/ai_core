// impl/cli.hpp — 設施：argv 驅動的小工具（接線解析 + dry-run 查詢）
//
// 兩個無狀態小函式，串起「entry 宣告 → 真實 I/O / 行為」：
//   1. resolve()    — 軸 1 B 接線解析：通道名(flag) + CLI args → 位址字串（terminal_binding 新家）。
//                     回傳的位址字串可直接餵 ac::io::read_all/write_all。
//   2. is_dry_run() — 軸 8 設施：掃 argv 有無 lib 約定的 `--dry-run` flag，回 bool。
//
// 風格比照 impl/intercept.hpp：直接掃 argv、無相依、注意邊界不越界。
#pragma once

#include <string>
#include <string_view>

namespace ac::cli {

// 軸 1 B 接線解析：找 `flag <value>`，命中回 value、否則回 fallback。
//
// 範例：resolve(argc, argv, "--input", "-")
//   無 --input          → "-"（fallback；read_all 解為 stdin）
//   --input foo.txt     → "foo.txt"（餵 read_all 即讀該檔）
//
// 邊界：flag 在最後一個 argv（沒有跟值）時不越界，視同未提供、回 fallback。
inline std::string resolve(int argc, char** argv, std::string_view flag,
                           std::string_view fallback) {
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == flag) {
      if (i + 1 < argc) {
        return std::string(argv[i + 1]);  // flag 的下一個 argv ＝ 位址字串
      }
      break;  // flag 在最後、無值 → 落到 fallback
    }
  }
  return std::string(fallback);
}

// 軸 8 設施：lib 統一約定乾跑 flag ＝ `--dry-run`（見 notes/axis_8_dry_run.md
// greenfield-wrap 模型：對外永遠用此標準 flag，brownfield 的 -n/--dry-run 由 wrapper 內部翻譯）。
// 純查詢，不改外部狀態；「不碰外部狀態」仍是程式本體邏輯。
inline bool is_dry_run(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--dry-run") {
      return true;
    }
  }
  return false;
}

}  // namespace ac::cli
