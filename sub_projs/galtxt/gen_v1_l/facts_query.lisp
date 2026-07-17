;;;; facts_query.lisp — 讀取側五動詞：resolve / match / trace / backrefs / layer。
;;;; 讀處處可讀（寫入門在 store／lod／tx）；一切回傳恆為插入序（確定性）。

;; resolve：先過視角 overlay，再沿 finer 鏈下行到當下最細值（LOD 讀取時解析）
;; 回傳雙值（value, id）或（nil, 原因）——對應 Lua 多值回傳
(defun db-resolve (db id &optional view)
  (multiple-value-bind (vid why)
      (funcall (view-map (or view (db-view db "全知"))) id)
    (cond ((null vid) (values nil why))
          ((placeholder-p vid) (values nil "佔位（尚未長出）"))
          (t (let ((f (or (gethash vid (db-by-id db)) (error "未知事實：~a" vid))))
               (loop while (fget f :finer)
                     do (setf f (gethash (fget f :finer) (db-by-id db))))
               (values (fget f :value) (fget f :id)))))))

;; match：圖模式查詢，pattern（keyword plist）欄位全等 AND；
;; 特例 (:who … :state "knows"/"hides") 查知識視圖
(defun db-match (db pattern &optional view)
  (if (getf pattern :state)
      (let ((k (get-knowledge db (getf pattern :who))))
        (copy-list (when k
                     (if (equal (getf pattern :state) "knows")
                         (kn-knows k)
                         (kn-hides k)))))
      (loop for f across (db-facts db)               ; 插入序
            when (and (loop for (key v) on pattern by #'cddr
                            always (equal (fget f key) v))
                      (or (null view) (funcall (view-map view) (fget f :id))))
              collect (fget f :id))))

;; trace：沿段（優先）或 refs 下行展開；佔位標記出來（接地檢查、導因鏈展開）
;; 每筆為 plist（:id :depth :placeholder）
(defun db-trace (db id &optional (depth 0))
  (let ((f (gethash id (db-by-id db))))
    (append (list (list :id id :depth depth
                        :placeholder (and (placeholder-p id) t)))
            (when f
              (cond ((fget f :segments)
                     (mapcan (lambda (s) (db-trace db s (1+ depth))) (fget f :segments)))
                    ((equal (fget f :kind) "連結")
                     (mapcan (lambda (r) (db-trace db r (1+ depth))) (fget f :refs))))))))

;; backrefs：誰指著我（refs 或 parent）——一致性傳播名單
(defun db-backrefs (db id)
  (loop for f across (db-facts db)
        when (or (and (equal (fget f :kind) "連結") (has-p (fget f :refs) id))
                 (equal (fget f :parent) id))
          collect (fget f :id)))

;; layer：節點＝0（基石）；連結＝所引用事實的最大層＋1（佔位不計）
(defun db-layer (db id)
  (let ((f (or (gethash id (db-by-id db)) (error "未知事實：~a" id))))
    (if (equal (fget f :kind) "節點")
        0
        (let ((top 0))
          (dolist (r (fget f :refs) (1+ top))
            (unless (placeholder-p r)
              (let ((l (db-layer db r)))
                (when (> l top) (setf top l)))))))))
