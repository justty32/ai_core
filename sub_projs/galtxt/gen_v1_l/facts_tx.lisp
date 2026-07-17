;;;; facts_tx.lisp — 執行期寫入門：speculate 交易（begin→apply→check→commit/rollback）。
;;;; DFS 分支＝begin→apply→check→rollback；整條 act 序列定案才 commit。
;;;; act＝plist：(:speaker … :act … :refs (id...) :effects (:reveal (id...) :to 角色))
;;;; Lua 版是閉包物件（tx.apply(...)）；CL 版用 struct＋tx-* 函式，語義一對一。

(defstruct tx db (acts nil))

;; 呈現＝鎖 canon。投影含粗事實與構件：連結呈現則其 refs 也算呈現、細值呈現則父鏈也算。
(defun present (db id)
  (unless (placeholder-p id)
    (let ((f (or (gethash id (db-by-id db)) (error "未知事實：~a" id))))
      (unless (fget f :canon)
        (let ((blocker (exclusion-block db id)))
          (when blocker
            (error "排斥邊擋下：~a 與已 canon 的 ~a 不可同真" id blocker)))
        (fset f :canon t)
        (when (fget f :parent) (present db (fget f :parent)))
        (when (equal (fget f :kind) "連結")
          (dolist (r (fget f :refs)) (present db r)))))))

(defun db-begin (db)
  (make-tx :db db))

(defun tx-apply (tx act)
  (setf (tx-acts tx) (append (tx-acts tx) (list act))))   ; 尾端追加：保持插入序

;; 回傳雙值（t）或（nil, 原因）——對應 Lua 的 false, why
(defun tx-check (tx)
  (let ((db (tx-db tx)))
    (dolist (act (tx-acts tx))
      (let ((k (get-knowledge db (getf act :speaker))))   ; 旁白等無視圖者不受「知道才能說」約束
        (dolist (r (getf act :refs))
          (when (placeholder-p r)
            (return-from tx-check (values nil (format nil "引用佔位事實（需先細化）：~a" r))))
          (unless (gethash r (db-by-id db))
            (return-from tx-check (values nil (format nil "引用不存在的事實：~a" r))))
          (when (and k (not (or (has-p (kn-knows k) r) (has-p (kn-hides k) r))))
            (return-from tx-check
              (values nil (format nil "矛盾防線：~a 說出了自己不知道的事實 ~a"
                                  (getf act :speaker) r))))
          (let ((blocker (exclusion-block db r)))
            (when (and blocker (not (equal blocker r)))
              (return-from tx-check
                (values nil (format nil "排斥邊：~a 與已 canon 的 ~a 不可同真" r blocker))))))
        (dolist (r (getf (getf act :effects) :reveal))
          (unless (and k (has-p (kn-hides k) r))
            (return-from tx-check
              (values nil (format nil "洩底無效：~a 沒有藏著 ~a" (getf act :speaker) r))))))))
  t)

(defun tx-commit (tx)
  (multiple-value-bind (ok why) (tx-check tx)
    (unless ok (error "寫入門拒絕：~a" why)))
  (let ((db (tx-db tx)))
    (dolist (act (tx-acts tx))
      (dolist (r (getf act :refs)) (present db r))
      (let ((k (get-knowledge db (getf act :speaker)))
            (eff (getf act :effects)))
        (dolist (r (getf eff :reveal))
          ;; 洩底＝知識轉移：hides→已表達（自己 knows）、聽者 knows 增加、誤會變體被真相替換
          (setf (kn-hides k) (remove-val (kn-hides k) r))
          (unless (has-p (kn-knows k) r)
            (setf (kn-knows k) (append (kn-knows k) (list r))))
          (let ((lk (and (getf eff :to) (get-knowledge db (getf eff :to)))))
            (when lk
              (unless (has-p (kn-knows lk) r)
                (setf (kn-knows lk) (append (kn-knows lk) (list r))))
              (setf (kn-variants lk)
                    (remove r (kn-variants lk) :key #'car :test #'equal))))
          (present db r)))))
  (setf (tx-acts tx) nil))

(defun tx-rollback (tx)
  (setf (tx-acts tx) nil))
