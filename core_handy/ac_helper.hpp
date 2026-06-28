// ac_helper.hpp — ai_core 設計的 C++ 濃縮 demo（header-only）
//
// 目標：把現有設計（九軸 metadata、register/intercept 等）一軸一軸
//       重新審視、濃縮成最嚴謹的 C++ 實作。每一步都經使用者確認。
//
// 狀態：九軸描述面定案落地（2026-06-27）。defs 型別見 defs/axes.hpp。
#pragma once

#include "defs/axes.hpp"       // 九軸描述性 metadata 型別（ac::Meta 等）
#include "impl/io.hpp"         // 軸 1 統一 I/O（read_all/write_all，batch 先）
#include "impl/state.hpp"      // 軸 4 狀態託管設施（StateStore；⊂ 軸 1 檔案分支）
#include "impl/cli.hpp"        // 設施：argv 接線解析（軸 1 B）＋ is_dry_run（軸 8）
#include "impl/meta_json.hpp"  // 膠水：Meta → --metadata JSON
#include "impl/intercept.hpp"  // 膠水：intercept(argc,argv,meta)
#include "impl/serve.hpp"      // 軸 2 serve：把 handler 託管成 Unix socket daemon
#include "impl/shell.hpp"      // 軸 1 #11 / brownfield-wrap：spawn 子程序、收 stdout/stderr+exit
#include "impl/json.hpp"       // 最小 JSON parser（抽 LLM 回應欄位；標準庫無 parser）
#include "impl/http.hpp"       // LLM 路徑地基：HTTP client（http=raw socket / https=curl shell-out）
#include "impl/llm.hpp"        // LLM backend：OpenAI 相容 chat（prompt→completion）
#include "impl/rate.hpp"       // 軸 5：consume-rate 計量（RateMeter）

namespace ac {

// 版本標記，證明 header 已被正確 include。
inline constexpr int helper_version = 0;

}  // namespace ac
