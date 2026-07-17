;;;; main.lisp — gen_v1_l 事實庫九示範跑批（Common Lisp 版，行為對齊 ../gen_v1/main.lua）。
;;;; 純 load 腳本風格（無 package／ASDF）：依序 load 子模組→seed→兩組示範。
;;;; 跑法：sbcl --script main.lisp

(defvar *here* (make-pathname :name nil :type nil :defaults *load-truename*))

(dolist (name '("facts" "facts_util" "facts_store" "facts_lod"
                "facts_query" "facts_tx" "seed" "demos_graph" "demos_gates"))
  (load (merge-pathnames (concatenate 'string name ".lisp") *here*)))

(let ((db (seed-db)))
  (demos-graph db)    ; ①～④：分層網／節點 LOD／連結 LOD／知識視圖
  (demos-gates db))   ; ⑤～⑨：寫入門／洩底／排斥邊／speculate／約束細化

(format t "~%九個示範全部通過——事實庫玩具版立住了（Common Lisp 版）。~%")
