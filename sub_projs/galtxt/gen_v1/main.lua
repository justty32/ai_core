-- main.lua — gen_v1 事實庫九示範跑批：素材（seed）＋兩組示範，各釘一條設計定調（00_設計.md）。
-- 單檔 ≤120 行慣例：素材與示範拆在 seed.lua / demos_graph.lua / demos_gates.lua。

local db = require("seed")
require("demos_graph")(db)   -- ①～④：分層網／節點 LOD／連結 LOD／知識視圖
require("demos_gates")(db)   -- ⑤～⑨：寫入門／洩底／排斥邊／speculate／約束細化

print("\n九個示範全部通過——事實庫玩具版立住了。")
