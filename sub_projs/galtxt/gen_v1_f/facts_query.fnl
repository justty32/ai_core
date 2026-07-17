;; facts_query.fnl — 讀取側五動詞：resolve / match / trace / backrefs / layer。
;; 讀處處可讀（寫入門在 store／lod／tx）；一切回傳恆為插入序（確定性）。

(local U (require :facts_util))
(local Q {})

;; resolve：先過視角 overlay，再沿 finer 鏈下行到當下最細值（LOD 讀取時解析）
(fn Q.resolve [self id view]
  (let [v (or view (self:view "全知"))
        (vid why) (v.map id)]
    (if (= vid nil) (values nil why)
        (U.is_placeholder vid) (values nil "佔位（尚未長出）")
        (do
          (var f (assert (. self.by_id vid) (.. "未知事實：" (tostring vid))))
          (while f.finer (set f (. self.by_id f.finer)))
          (values f.value f.id)))))

;; match：圖模式查詢，pattern 欄位全等 AND；特例 {who=…, state="knows"/"hides"} 查知識視圖
(fn Q.match [self pattern view]
  (if pattern.state
      (let [k (or (. self.knowledge pattern.who) {})
            out []]
        (each [_ id (ipairs (or (. k pattern.state) []))]
          (table.insert out id))
        out)
      (let [out []]
        (each [_ f (ipairs self.list)]
          (var ok true)
          (each [key v (pairs pattern)]
            (when (not= (. f key) v) (set ok false)))
          (when (and ok (or (= view nil) (view.map f.id)))
            (table.insert out f.id)))
        out)))

;; trace：沿段（優先）或 refs 下行展開；佔位標記出來（接地檢查、導因鏈展開）
(fn Q.trace [self id out depth]
  (let [out (or out [])
        depth (or depth 0)]
    (table.insert out { :id id :depth depth :placeholder (or (U.is_placeholder id) nil) })
    (let [f (. self.by_id id)]
      (when f
        (if f.segments
            (each [_ s (ipairs f.segments)] (self:trace s out (+ depth 1)))
            (= f.kind "連結")
            (each [_ r (ipairs f.refs)] (self:trace r out (+ depth 1))))))
    out))

;; backrefs：誰指著我（refs 或 parent）——一致性傳播名單
(fn Q.backrefs [self id]
  (let [out []]
    (each [_ f (ipairs self.list)]
      (when (or (and (= f.kind "連結") (U.has f.refs id)) (= f.parent id))
        (table.insert out f.id)))
    out))

;; layer：節點＝0（基石）；連結＝所引用事實的最大層＋1（佔位不計）
(fn Q.layer [self id]
  (let [f (assert (. self.by_id id) (.. "未知事實：" (tostring id)))]
    (if (= f.kind "節點")
        0
        (do
          (var top 0)
          (each [_ r (ipairs f.refs)]
            (when (not (U.is_placeholder r))
              (let [l (self:layer r)]
                (when (> l top) (set top l)))))
          (+ top 1)))))

Q
