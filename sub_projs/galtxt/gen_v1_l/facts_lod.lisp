;;;; facts_lod.lisp — 編譯期寫入門②：db-refine（細化）。柱三＋柱四的 LOD 紀律在此把關。
;;;; 三種形態（spec 為 keyword plist）：
;;;;   節點取點   spec=(:value … :id …)        → 父約束（range）內取點，父 :finer 指向細層
;;;;   連結加段   spec=(:segments (連結id...)) → 鏈首尾＝粗邊首尾、段段相接（不推翻粗邊語義）
;;;;   約束滿足   spec=(:constraints (k v …))  → 在既有子事實裡找滿足點；找不到→掛起工單

(defun db-refine (db parent-id spec)
  (let ((p (or (gethash parent-id (db-by-id db)) (error "未知事實：~a" parent-id))))

    (let ((segs (getf spec :segments)))
      (when segs
        (unless (equal (fget p :kind) "連結")
          (error "只有連結能加段：~a" parent-id))
        (let ((fst (gethash (first segs) (db-by-id db)))
              (lst (gethash (car (last segs)) (db-by-id db))))
          (unless (and (equal (first (fget fst :refs)) (first (fget p :refs)))
                       (equal (car (last (fget lst :refs))) (car (last (fget p :refs)))))
            (error "段鏈首尾必須等於粗邊首尾（多段不得推翻粗連結語義）：~a" parent-id)))
        (loop for (a-id b-id) on segs while b-id do
          (let ((a (gethash a-id (db-by-id db)))
                (b (gethash b-id (db-by-id db))))
            (unless (equal (car (last (fget a :refs))) (first (fget b :refs)))
              (error "段鏈不連續：~a → ~a" a-id b-id))))
        (fset p :segments segs)
        (return-from db-refine parent-id)))

    (let ((constraints (getf spec :constraints)))
      (when constraints
        (loop for f across (db-facts db)
              when (and (equal (fget f :parent) parent-id)
                        (loop for (k v) on constraints by #'cddr
                              always (equal (fget f k) v)))
                do (return-from db-refine (fget f :id)))   ; 插入序首個滿足者（確定性）
        (vector-push-extend (list :parent parent-id
                                  :constraints constraints :grain (getf spec :grain))
                            (db-pending db))
        (return-from db-refine
          (values nil (format nil "掛起工單#~d" (fill-pointer (db-pending db)))))))

    ;; 節點取點
    (when (fget p :finer)
      (error "已有細層：~a（要更細請對細層再 refine）" (fget p :finer)))
    (let ((range (fget p :range))
          (value (getf spec :value)))
      (when range
        (unless (and (numberp value) (<= (first range) value (second range)))
          (error "細化只能落在父約束內：~a ∉ [~d,~d]" value (first range) (second range)))))
    (let ((id (or (getf spec :id) (error "節點取點需給 id"))))
      (db-add db (fact :id id :kind (fget p :kind) :parent parent-id :value (getf spec :value)))
      (fset p :finer id)   ; canon 事實也可細化（canon 鎖只擋 modify）
      id)))
