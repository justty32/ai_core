;;;; facts_store.lisp — 編譯期寫入門①（db-add 入庫）＋ db-modify（canon 鎖）＋知識視圖＋dump。
;;;; 總則見 facts.lisp：確定性遍歷走插入序向量；不變式架在門上。

;;; ------------------------------------------------------------- 編譯期寫入門①：add（入庫）

;; 檢查：ID 唯一、連結必有 refs、refs 必存在（佔位豁免）→ 接地不變式由歸納保證
(defun db-add (db f)
  (let ((id (fget f :id)))
    (unless (and id (not (gethash id (db-by-id db))))
      (error "ID 缺失或重複：~a" id))
    (unless (member (fget f :kind) '("節點" "連結") :test #'equal)
      (error "kind 必須是 節點/連結：~a" (fget f :kind)))
    (when (equal (fget f :kind) "連結")
      (let ((refs (fget f :refs)))
        (unless (and (consp refs) (>= (length refs) 1))
          (error "連結必須有 refs：~a" id))
        (dolist (r refs)
          (unless (or (placeholder-p r) (gethash r (db-by-id db)))
            (error "接地失敗：refs 指向不存在的事實 ~a（佔位請用 ? 前綴）" r)))))
    (when (fget f :parent)
      (unless (gethash (fget f :parent) (db-by-id db))
        (error "parent 指向不存在的事實：~a" (fget f :parent))))
    ;; canon 缺欄即 nil（對應 Lua 的 f.canon = f.canon or false；nil≡false）
    (vector-push-extend f (db-facts db))
    (setf (gethash id (db-by-id db)) f)
    id))

;;; ------------------------------------------------------------- modify（未呈現可改；canon 鎖）

(defun db-modify (db id value)
  (let ((f (or (gethash id (db-by-id db)) (error "未知事實：~a" id))))
    (when (fget f :canon)
      (error "canon 鎖：已呈現的事實不可改（只可再細化）：~a" id))
    (fset f :value value)))

;;; ------------------------------------------------------------- 知識視圖（柱二）

(defun db-set-knowledge (db who tbl)
  "tbl 為 plist（:knows :hides :variants）；variants 為 alist（真相id . 變體id）。"
  (let ((k (make-kn :knows (copy-list (getf tbl :knows))
                    :hides (copy-list (getf tbl :hides))
                    :variants (copy-alist (getf tbl :variants))))
        (cell (assoc who (db-knowledge db) :test #'equal)))
    (if cell
        (setf (cdr cell) k)
        (setf (db-knowledge db)                    ; 尾端追加：保持插入序（確定性）
              (append (db-knowledge db) (list (cons who k)))))))

;; 視角＝overlay：全知恆等；角色＝variants 先替換（誤會）、knows∪hides 可見、其餘不可見
;; map 回傳雙值（vid, why）——對應 Lua 的多值回傳 nil, "不可見"
(defstruct view who map)

(defun db-view (db who)
  (if (equal who "全知")
      (make-view :who who :map (lambda (id) (values id nil)))
      (let ((k (or (get-knowledge db who)
                   (error "沒有這個角色的知識視圖：~a" who))))
        (make-view :who who :map
          (lambda (id)
            (let ((var (cdr (assoc id (kn-variants k) :test #'equal))))
              (cond (var (values var nil))         ; 誤會：映到錯誤變體
                    ((or (has-p (kn-knows k) id) (has-p (kn-hides k) id)) (values id nil))
                    (t (values nil "不可見")))))))))

;;; ------------------------------------------------------------- dump（測試用：全庫確定性快照）

(defun db-dump (db)
  (let ((parts nil))
    (loop for f across (db-facts db) do             ; 插入序
      (push (format nil "~a|~a|~a|~a"
                    (fget f :id) (fget f :value) (fget f :canon) (fget f :finer))
            parts))
    (dolist (who (sort (mapcar #'car (db-knowledge db)) #'string<))  ; 排序後再輸出：快照也要確定性
      (let ((k (get-knowledge db who)))
        (push (format nil "~a|~{~a~^,~}|~{~a~^,~}" who (kn-knows k) (kn-hides k)) parts)))
    (format nil "~{~a~^~%~}" (nreverse parts))))
