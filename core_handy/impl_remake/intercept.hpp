// impl_remake/intercept.hpp — defs 的實現（其二）：--metadata 顯式攔截進入點
//
// register/intercept 模型的 C++ 對應（見 src/ai_core/_core.py）：
//   純宣告（Meta）零副作用；--metadata 的生效靠在 main() 顯式呼叫 intercept()。
//
// 自足版：直接寫 stdout（不拉設施層）。本檔只實現 defs 的「--metadata 攔截」這一行為，
// 不引入 impl/ 的統一 I/O 設施。
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

#include "../defs/axes.hpp"
#include "meta_json.hpp"

namespace ac {

// 掃 argv 找 --metadata：命中則序列化 Meta 成 JSON、寫 stdout、exit(0)；
// 否則 return（把控制交還給程式本體）。
inline void intercept(int argc, char** argv, const Meta& meta) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--metadata") {
      const std::string out = to_metadata_json(meta) + "\n";
      std::fwrite(out.data(), 1, out.size(), stdout);
      std::exit(0);
    }
  }
}

}  // namespace ac
