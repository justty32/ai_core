;;;; seed.lisp — 建材期素材：河鹿堂定點（柏木秋穗 × 佐伯悠）灌進事實庫，回傳 db。
;;;; 全部走編譯期門①（db-add）；示範在 demos_*.lisp。

(defun seed-db ()
  (let ((db (make-db)))

    ;; 第 0 層：節點（基石）
    (db-add db (fact :id "人#柏木秋穗" :kind "節點" :value "柏木秋穗"))
    (db-add db (fact :id "人#佐伯悠"   :kind "節點" :value "佐伯悠"))
    (db-add db (fact :id "物#書417"    :kind "節點" :value "推理文庫・絕版短篇集"))
    (db-add db (fact :id "場#河鹿堂"   :kind "節點" :value "舊書店・河鹿堂"))
    (db-add db (fact :id "場#校園後方" :kind "節點" :value "校園後方（粗）"))
    (db-add db (fact :id "時#年代"     :kind "節點" :value "90年代"
                     :form "區間" :range (list 1990 1999)))
    (db-add db (fact :id "事#圖書館"   :kind "節點" :value "去年在圖書館他替她解了圍"))
    (db-add db (fact :id "態#書在架"   :kind "節點" :value "書#417 還在店內書架"))
    (db-add db (fact :id "態#書已售"   :kind "節點" :value "書#417 已售出"))

    ;; 第 1 層：連結（連結也是事實：有 ID、有值、可入知識視圖）
    (db-add db (fact :id "邊#她心動" :kind "連結" :edge-type "心動"
                     :refs (list "人#柏木秋穗" "人#佐伯悠") :value "她對他心動"))
    (db-add db (fact :id "邊#誤會"   :kind "連結" :edge-type "嫌惡"
                     :refs (list "人#柏木秋穗" "人#佐伯悠") :value "她討厭他（他的誤會）"))
    (db-add db (fact :id "邊#常客"   :kind "連結" :edge-type "常客"
                     :refs (list "人#佐伯悠" "場#河鹿堂") :value "他是河鹿堂常客"))
    (db-add db (fact :id "邊#排斥書態" :kind "連結" :edge-type "排斥"
                     :refs (list "態#書在架" "態#書已售")))

    ;; 第 2 層：連結的連結（戲劇結構住高層：真相與誤會的張力）
    (db-add db (fact :id "邊#認知落差" :kind "連結" :edge-type "張力"
                     :refs (list "邊#她心動" "邊#誤會")
                     :value "真相與誤會的落差＝本線戲劇引擎"))

    ;; 知識視圖（柱二）：她藏著心動；他知道的那條是誤會變體
    (db-set-knowledge db "柏木秋穗"
      (list :knows (list "人#柏木秋穗" "人#佐伯悠" "物#書417" "場#河鹿堂"
                         "事#圖書館" "態#書在架")
            :hides (list "邊#她心動")))
    (db-set-knowledge db "佐伯悠"
      (list :knows (list "人#柏木秋穗" "人#佐伯悠" "物#書417" "場#河鹿堂"
                         "事#圖書館" "邊#誤會" "態#書在架")
            :variants (list (cons "邊#她心動" "邊#誤會"))))  ; 誤會＝視圖裡映到錯誤變體

    db))
