;;;; demos_gates.lisp — 示範⑤～⑨：執行期寫入門／洩底／排斥邊／speculate／約束細化。全數自帶 assert。

(defun demos-gates (db)

  ;; ---------------------------------------------- ⑤ 執行期寫入門：說不出不知道的事；呈現→canon 鎖
  (let ((tx (db-begin db)))
    (tx-apply tx (list :speaker "佐伯悠" :act "評書" :refs (list "物#書417" "邊#她心動")))
    (multiple-value-bind (pass why) (tx-check tx)
      (assert (and (not pass) (search "矛盾防線" why)) () "⑤ 他引用真相邊應被矛盾防線擋下"))
    (tx-rollback tx))

  (let ((tx (db-begin db)))
    (tx-apply tx (list :speaker "佐伯悠" :act "評書" :refs (list "物#書417" "邊#誤會")))
    (assert (tx-check tx))
    (tx-commit tx))
  (assert (and (fget (gethash "物#書417" (db-by-id db)) :canon)
               (fget (gethash "邊#誤會" (db-by-id db)) :canon))
          () "⑤ 呈現過即 canon")

  (assert (expect-error (db-modify db "物#書417" "普通文庫")) () "⑤ canon 鎖：已呈現不可改")
  (db-modify db "態#書已售" "書#417 已售出（改：被人預訂）")   ; 未呈現可改，玩家無感
  (format t "⑤ 寫入門：矛盾防線＋canon 鎖（呈現才鎖、未呈現可改）　OK~%")

  ;; ---------------------------------------------- ⑥ 洩底＝知識轉移：hides→已表達、誤會被真相替換
  (let ((tx (db-begin db)))
    (tx-apply tx (list :speaker "柏木秋穗" :act "洩底" :refs (list "邊#她心動")
                       :effects (list :reveal (list "邊#她心動") :to "佐伯悠")))
    (assert (tx-check tx))
    (tx-commit tx))

  (assert (= (length (db-match db (list :who "柏木秋穗" :state "hides"))) 0)
          () "⑥ 洩底後 hides 清空（恰一記帳可據此計數）")
  (assert (equal (db-resolve db "邊#她心動" (db-view db "佐伯悠")) "她對他心動")
          () "⑥ 誤會拆除：他的視圖同一 ID 改解析到真相")
  (assert (fget (gethash "邊#她心動" (db-by-id db)) :canon) () "⑥ 洩了底＝呈現過＝canon")
  (format t "⑥ 洩底＝知識轉移＋誤會拆除　OK~%")

  ;; ---------------------------------------------- ⑦ 排斥邊：兩端不得同真
  (let ((tx (db-begin db)))
    (tx-apply tx (list :speaker "旁白" :act "描寫" :refs (list "態#書在架")))
    (assert (tx-check tx))
    (tx-commit tx))

  (let ((tx (db-begin db)))
    (tx-apply tx (list :speaker "旁白" :act "描寫" :refs (list "態#書已售")))
    (multiple-value-bind (pass why) (tx-check tx)
      (assert (and (not pass) (search "排斥邊" why)) () "⑦ 在架已 canon，已售不得再呈現"))
    (tx-rollback tx))
  (format t "⑦ 排斥邊：矛盾檢查＝圖結構查詢　OK~%")

  ;; ---------------------------------------------- ⑧ speculate：試探分支不落盤；同查詢逐位元相同
  (let ((snap1 (db-dump db))
        (tx (db-begin db)))
    (tx-apply tx (list :speaker "旁白" :act "描寫" :refs (list "場#河鹿堂")))
    (assert (tx-check tx))
    (tx-rollback tx)                    ; DFS 退出分支
    (assert (equal (db-dump db) snap1) () "⑧ rollback 後全庫快照逐位元相同"))

  (let ((m1 (format nil "~{~a~^;~}" (db-match db (list :kind "連結"))))
        (m2 (format nil "~{~a~^;~}" (db-match db (list :kind "連結")))))
    (assert (and (equal m1 m2) (not (equal m1 ""))) () "⑧ match 恆為插入序（確定性）"))
  (format t "⑧ speculate 分支不落盤＋查詢確定性　OK~%")

  ;; ---------------------------------------------- ⑨ 約束細化：解不出→掛起工單（編譯期 LLM 崗位）
  (multiple-value-bind (got ticket)
      (db-refine db "場#校園後方" (list :grain "場所級" :constraints (list :mood "櫻花")))
    (assert (and (null got) (search "掛起工單" ticket)) () "⑨ 庫裡沒有滿足點→掛起"))
  (assert (= (fill-pointer (db-pending db)) 1))

  ;; 編譯期補件（LLM 炸候選→人審→入庫的入庫那一步；分佈未鎖，世界為故事讓路）
  (db-add db (fact :id "場#櫻花樹下" :kind "節點" :parent "場#校園後方"
                   :mood "櫻花" :value "校園後方・櫻花樹下"))
  (assert (equal (db-refine db "場#校園後方"
                            (list :grain "場所級" :constraints (list :mood "櫻花")))
                 "場#櫻花樹下")
          () "⑨ 補件後同一查詢解出告白級地點")
  (format t "⑨ 約束細化＋掛起工單（告白要在櫻花樹下）　OK~%"))
