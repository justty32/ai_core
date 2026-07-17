;; facts_util.fnl — facts 子模組共用小工具（無狀態純函式）。

(local U {})

(fn U.has [arr v]
  (var found false)
  (each [_ x (ipairs arr)]
    (when (= x v) (set found true)))
  found)

(fn U.remove_val [arr v]
  (var idx nil)
  (each [i x (ipairs arr)]
    (when (and (= x v) (= idx nil)) (set idx i)))
  (when idx (table.remove arr idx)))

;; 佔位連結：refs 裡「?」開頭＝連到還不存在的事實（懸空合法，柱四）
(fn U.is_placeholder [id]
  (and (= (type id) "string") (= (id:sub 1 1) "?")))

;; 排斥邊檢查：有無「排斥」邊連著 id、而另一端已 canon？（柱四：兩端不得同真）
;; 回傳插入序首個 blocker（確定性）
(fn U.exclusion_block [db id]
  (var blocker nil)
  (each [_ f (ipairs db.list)]
    (when (and (= blocker nil) (= f.kind "連結") (= f.edge_type "排斥")
               (U.has f.refs id))
      (each [_ other (ipairs f.refs)]
        (when (and (= blocker nil) (not= other id)
                   (. db.by_id other) (. db.by_id other :canon))
          (set blocker other)))))
  blocker)

U
