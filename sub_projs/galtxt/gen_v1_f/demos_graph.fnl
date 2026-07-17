;; demos_graph.fnl — 示範①～④：分層網／節點 LOD／連結 LOD／知識視圖。全數自帶 assert。

(fn run [db]

  ;;; -------------------- ① 分層網：節點 0、連結 1、連結的連結 2；接地不變式
  (assert (= (db:layer "人#柏木秋穗") 0))
  (assert (= (db:layer "邊#她心動") 1))
  (assert (= (db:layer "邊#認知落差") 2))

  (let [(ok) (pcall #(db:add { :id "邊#壞" :kind "連結" :edge_type "導因於"
                               :refs ["邊#她心動" "事#不存在"] }))]
    (assert (not ok) "① refs 指向不存在事實應被接地檢查擋下"))

  ;; 懸空連結合法：佔位「?」＝backstory 需求記號（為什麼心動？之後再長）
  (db:add { :id "邊#心動導因" :kind "連結" :edge_type "導因於"
            :refs ["邊#她心動" "?事#心動的更早源頭"] })
  (let [tr (db:trace "邊#心動導因")]
    (assert (. tr (length tr) :placeholder) "① 佔位事實應在 trace 中被標記"))
  (print "① 分層網＋接地不變式＋懸空佔位　OK")

  ;;; -------------------- ② 節點 LOD：粗＝約束區間、細化＝區間內取點
  (assert (= (db:resolve "時#年代") "90年代") "② 未細化時解析到粗值")

  (db:refine "時#年代" { :value 1996 :id "時#1996" })
  (assert (= (db:resolve "時#年代") 1996)
          "② 細化後同一 ID 解析到細值（連結指向粗事實拿當下最細 canon 值）")

  (let [(ok) (pcall #(db:refine "時#年代" { :value 2005 :id "時#壞" }))]
    (assert (not ok) "② 已有細層／出區間的細化應被擋（此處命中「已有細層」）"))
  (print "② 節點 LOD：父約束內取點、resolve 拿最細值　OK")

  ;;; -------------------- ③ 連結 LOD：加中繼段（多段不推翻粗邊語義）
  (db:add { :id "邊#銘記" :kind "連結" :edge_type "銘記"
            :refs ["人#柏木秋穗" "事#圖書館"] :value "她一直記著那件事" })
  (db:add { :id "邊#解圍" :kind "連結" :edge_type "當事者"
            :refs ["事#圖書館" "人#佐伯悠"] :value "出手的人是他" })

  (let [(ok) (pcall #(db:refine "邊#她心動" { :segments ["邊#銘記" "邊#常客"] }))]
    (assert (not ok) "③ 段鏈首尾/連續性不符應被擋"))

  (db:refine "邊#她心動" { :segments ["邊#銘記" "邊#解圍"] })
  (let [tr (db:trace "邊#她心動")]
    ;; 展開＝粗邊＋兩段、每段再下行到自己的 refs（3＋2×2＝7 節點）
    (assert (and (= (length tr) 7)
                 (= (. tr 2 :id) "邊#銘記")
                 (= (. tr 5 :id) "邊#解圍"))
            "③ trace 沿段展開：粗邊→銘記→(事件)→解圍"))
  (print "③ 連結 LOD：粗邊展開多段、首尾語義守住　OK")

  ;;; -------------------- ④ 知識視圖＝overlay：hides 查詢、誤會變體、不可見
  (let [hidden (db:match { :who "柏木秋穗" :state "hides" })]
    (assert (and (= (length hidden) 1) (= (. hidden 1) "邊#她心動"))
            "④ 「給我她現在可洩的底」"))

  (assert (= (db:resolve "邊#她心動" (db:view "柏木秋穗")) "她對他心動") "④ 她自己看：真相")
  (assert (= (db:resolve "邊#她心動" (db:view "佐伯悠")) "她討厭他（他的誤會）")
          "④ 他看同一 ID：解析到誤會變體")

  (let [(invisible why) (db:resolve "邊#銘記" (db:view "佐伯悠"))]
    (assert (and (= invisible nil) (= why "不可見")) "④ 他不知道她的銘記邊"))
  (print "④ 知識視圖 overlay：真相/誤會/不可見三態　OK"))

run
