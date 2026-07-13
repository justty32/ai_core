// impl/intercept.hpp — 膠水：--metadata 顯式攔截進入點
//
// register/intercept 模型的 C++ 對應（見 src/ai_core/_core.py）：
// 純宣告（Meta）零副作用；--metadata 的生效靠在 main() 顯式呼叫 intercept()。
#pragma once

#include <cstdlib>
#include <string>

#include "../defs/axes.hpp"
#include "io.hpp"
#include "meta_json.hpp"

namespace ac {

// 掃 argv 找 --metadata：命中則序列化 Meta 成 JSON、寫 stdout、exit(0)；
// 否則 return（把控制交還給程式本體）。
inline void intercept(int argc, char** argv, const Meta& meta) {
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--metadata") {
      io::write_all("-", to_metadata_json(meta) + "\n");
      std::exit(0);
    }
  }
}

}  // namespace ac
