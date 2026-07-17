;;;; demos_graph.lisp — 示範①～④：分層網／節點 LOD／連結 LOD／知識視圖。全數自帶 assert。

(defun demos-graph (db)

  ;; ---------------------------------------------- ① 分層網：節點 0、連結 1、連結的連結 2；接地不變式
  (assert (= (db-layer db "人#柏木秋穗") 0))
  (assert (= (db-layer db "邊#她心動") 1))
  (assert (= (db-layer db "邊#認知落差") 2))

  (assert (expect-error
            (db-add db (fact :id "邊#壞" :kind "連結" :edge-type "導因於"
                             :refs (list "邊#她心動" "事#不存在"))))
          () "① refs 指向不存在事實應被接地檢查擋下")

  ;; 懸空連結合法：佔位「?」＝backstory 需求記號（為什麼心動？之後再長）
  (db-add db (fact :id "邊#心動導因" :kind "連結" :edge-type "導因於"
                   :refs (list "邊#她心動" "?事#心動的更早源頭")))
  (let ((tr (db-trace db "邊#心動導因")))
    (assert (getf (car (last tr)) :placeholder) () "① 佔位事實應在 trace 中被標記"))
  (format t "① 分層網＋接地不變式＋懸空佔位　OK~%")

  ;; ---------------------------------------------- ② 節點 LOD：粗＝約束區間、細化＝區間內取點
  (assert (equal (db-resolve db "時#年代") "90年代") () "② 未細化時解析到粗值")

  (db-refine db "時#年代" (list :value 1996 :id "時#1996"))
  (assert (equal (db-resolve db "時#年代") 1996)
          () "② 細化後同一 ID 解析到細值（連結指向粗事實拿當下最細 canon 值）")

  (assert (expect-error (db-refine db "時#年代" (list :value 2005 :id "時#壞")))
          () "② 已有細層／出區間的細化應被擋（此處命中「已有細層」）")
  (format t "② 節點 LOD：父約束內取點、resolve 拿最細值　OK~%")

  ;; ---------------------------------------------- ③ 連結 LOD：加中繼段（多段不推翻粗邊語義）
  (db-add db (fact :id "邊#銘記" :kind "連結" :edge-type "銘記"
                   :refs (list "人#柏木秋穗" "事#圖書館") :value "她一直記著那件事"))
  (db-add db (fact :id "邊#解圍" :kind "連結" :edge-type "當事者"
                   :refs (list "事#圖書館" "人#佐伯悠") :value "出手的人是他"))

  (assert (expect-error   ; 鏈不連續且尾不對
            (db-refine db "邊#她心動" (list :segments (list "邊#銘記" "邊#常客"))))
          () "③ 段鏈首尾/連續性不符應被擋")

  (db-refine db "邊#她心動" (list :segments (list "邊#銘記" "邊#解圍")))
  (let ((tr (db-trace db "邊#她心動")))
    ;; 展開＝粗邊＋兩段、每段再下行到自己的 refs（3＋2×2＝7 節點）
    (assert (and (= (length tr) 7)
                 (equal (getf (nth 1 tr) :id) "邊#銘記")
                 (equal (getf (nth 4 tr) :id) "邊#解圍"))
            () "③ trace 沿段展開：粗邊→銘記→(事件)→解圍"))
  (format t "③ 連結 LOD：粗邊展開多段、首尾語義守住　OK~%")

  ;; ---------------------------------------------- ④ 知識視圖＝overlay：hides 查詢、誤會變體、不可見
  (let ((hidden (db-match db (list :who "柏木秋穗" :state "hides"))))
    (assert (and (= (length hidden) 1) (equal (first hidden) "邊#她心動"))
            () "④ 「給我她現在可洩的底」"))

  (let ((truth  (db-resolve db "邊#她心動" (db-view db "柏木秋穗")))
        (belief (db-resolve db "邊#她心動" (db-view db "佐伯悠"))))
    (assert (equal truth "她對他心動") () "④ 她自己看：真相")
    (assert (equal belief "她討厭他（他的誤會）") () "④ 他看同一 ID：解析到誤會變體"))

  (multiple-value-bind (invisible why)
      (db-resolve db "邊#銘記" (db-view db "佐伯悠"))
    (assert (and (null invisible) (equal why "不可見")) () "④ 他不知道她的銘記邊"))
  (format t "④ 知識視圖 overlay：真相/誤會/不可見三態　OK~%"))
