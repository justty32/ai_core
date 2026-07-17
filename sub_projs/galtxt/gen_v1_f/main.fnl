;; main.fnl — gen_v1_f 事實庫九示範跑批（Fennel 版，行為對齊 gen_v1）。
;; 單檔 ≤120 行慣例：素材與示範拆在 seed.fnl / demos_graph.fnl / demos_gates.fnl。

(local db (require :seed))
((require :demos_graph) db)   ; ①～④：分層網／節點 LOD／連結 LOD／知識視圖
((require :demos_gates) db)   ; ⑤～⑨：寫入門／洩底／排斥邊／speculate／約束細化

(print "\n九個示範全部通過——事實庫玩具版立住了（Fennel 版）。")
