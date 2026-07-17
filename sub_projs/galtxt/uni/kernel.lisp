;;;; kernel.lisp — 統一節點 kernel（自 uniform_probe/synth 提純）。一種節點跑所有粒度。
;;;; 八股/段/句式/填充/原子事實＝同一 (node …)｜(fact …)；粒度由 refs 湧現、不限四種。

(defvar *ns* (make-hash-table :test 'eq))          ; id → plist：(:kids … :fn … | :真值 t :述 …)
(defvar *env* nil)                                 ; 唯一輸入：母題/情緒/選路 alist
(defun env (k) (cdr (assoc k *env*)))
(defun nl () (coerce '(#\Newline) 'string))

;;; 編譯期：把一個 part 折成「回字串」的表達式。
;;; 字串=字面｜關鍵字=換行｜symbol=子引用(遞迴 rz)｜(? key a b…)=依 env[key] 選一(選詞＝分支)｜
;;; (! form)=逃生艙(任意 lisp 回字串，帶邏輯的填充/母題導出走這)
(eval-when (:compile-toplevel :load-toplevel :execute)
  (defun %p (x)
    (cond ((stringp x) x)
          ((keywordp x) '(nl))
          ((symbolp x) `(rz ',x))
          ((eq (car x) '?) `(funcall (nth (mod (or (env ,(cadr x)) 0) ,(length (cddr x)))
                                          (list ,@(mapcar (lambda (o) `(lambda () ,(%p o))) (cddr x))))))
          ((eq (car x) '!) (cadr x))
          (t (error "bad part ~a" x))))
  (defun %kids (ps)
    (let (o) (labels ((w (x) (cond ((and (symbolp x) (not (keywordp x)) (not (member x '(? !)))) (pushnew x o))
                                   ((consp x) (mapc #'w x)))))
               (mapc #'w ps)) (nreverse o))))

;;; 唯一寫入端：node（字面域，realize 回字串）／fact（真值域，帶命題），共用一張表
(defmacro node (id &rest parts)
  `(setf (gethash ',id *ns*)
         (list :kids ',(%kids parts) :fn (lambda () (concatenate 'string ,@(mapcar #'%p parts))))))
(defmacro fact (id 述 &rest 引)
  `(setf (gethash ',id *ns*) (list :kids ',引 :真值 t :述 ,述)))

;;; 讀取端：全是小函式，皆從同一表湧現
(defun nd (id) (gethash id *ns*))
(defun rz (id) (funcall (getf (nd id) :fn)))
(defun kids (id) (getf (nd id) :kids))
(defun 懸空? (id) (null (nd id)))                              ; 引到未定義＝佔位需求
(defun 真值? (id) (getf (nd id) :真值))                        ; 域界線：事實 vs 字面
(defun 層 (id)                                                 ; 粒度＝1+max(子層)、葉=0（湧現，不限四種）
  (let ((k (remove-if #'懸空? (kids id))))
    (if (null k) 0 (1+ (reduce #'max (mapcar #'層 k))))))
