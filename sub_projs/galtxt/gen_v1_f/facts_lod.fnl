;; facts_lod.fnl — 編譯期寫入門②：refine（細化）。柱三＋柱四的 LOD 紀律在此把關。
;; 三種形態：
;;   節點取點   spec={ value=…, id=… }        → 父約束（range）內取點，父.finer 指向細層
;;   連結加段   spec={ segments=[連結id…] }   → 鏈首尾＝粗邊首尾、段段相接（不推翻粗邊語義）
;;   約束滿足   spec={ constraints={k v…} }   → 在既有子事實裡找滿足點；找不到→掛起工單

(local L {})

(fn refine-segments [self p parent_id segs]
  (assert (= p.kind "連結") (.. "只有連結能加段：" parent_id))
  (let [head (. self.by_id (. segs 1))
        tail (. self.by_id (. segs (length segs)))]
    (when (or (not= (. head.refs 1) (. p.refs 1))
              (not= (. tail.refs (length tail.refs)) (. p.refs (length p.refs))))
      (error (.. "段鏈首尾必須等於粗邊首尾（多段不得推翻粗連結語義）：" parent_id) 0))
    (for [i 1 (- (length segs) 1)]
      (let [a (. self.by_id (. segs i))
            b (. self.by_id (. segs (+ i 1)))]
        (when (not= (. a.refs (length a.refs)) (. b.refs 1))
          (error (.. "段鏈不連續：" (. segs i) " → " (. segs (+ i 1))) 0))))
    (tset p :segments segs)
    parent_id))

(fn refine-constraints [self parent_id spec]
  (var found nil)
  (each [_ f (ipairs self.list)]                 ; 插入序首個滿足者（確定性）
    (when (and (= found nil) (= f.parent parent_id))
      (var ok true)
      (each [k v (pairs spec.constraints)]
        (when (not= (. f k) v) (set ok false)))
      (when ok (set found f.id))))
  (if found
      found
      (do (table.insert self.pending
            { :parent parent_id :constraints spec.constraints :grain spec.grain })
          (values nil (.. "掛起工單#" (length self.pending))))))

(fn refine-point [self p parent_id spec]
  (when p.finer
    (error (.. "已有細層：" p.finer "（要更細請對細層再 refine）") 0))
  (when p.range
    (when (or (not= (type spec.value) "number")
              (< spec.value (. p.range 1)) (> spec.value (. p.range 2)))
      (error (string.format "細化只能落在父約束內：%s ∉ [%d,%d]"
               (tostring spec.value) (. p.range 1) (. p.range 2)) 0)))
  (let [id (assert spec.id "節點取點需給 id")]
    (self:add { :id id :kind p.kind :parent parent_id :value spec.value })
    (tset p :finer id)   ; canon 事實也可細化（canon 鎖只擋 modify）
    id))

(fn L.refine [self parent_id spec]
  (let [p (assert (. self.by_id parent_id) (.. "未知事實：" (tostring parent_id)))]
    (if spec.segments    (refine-segments self p parent_id spec.segments)
        spec.constraints (refine-constraints self parent_id spec)
        (refine-point self p parent_id spec))))

L
