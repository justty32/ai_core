;;;; search.lisp — 共用 search kernel：搜 *env*（風格軸）使成本最小。
;;;; 這就是 gen_v0 引擎 ≅ realizer director 的統一實現（見 ../一台搜尋機.md）：
;;;; 「枚舉離散空間→護欄剪枝→成本(prior＋intent＋history)排序→取最小→history 迭代多樣」。
;;;; 母題是內容輸入（不搜）；風格軸 *axes* 是被搜對象。因 env 鍵跨節點共享＝耦合，故全域枚舉。

(defparameter *axes* '((:move . 3) (:emo . 3)))               ; 可搜風格軸與定義域大小
(defparameter *非canon* '("咖啡廳" "陽溜亭" "天台" "教室"))
(defun 有非canon? (s) (some (lambda (w) (search w s)) *非canon*))

(defun icost (intent s)                                       ; 風格意圖成本
  (case intent (:甜 (if (search "……啊" s) 0 6))
               (:靜 (floor (length s) 8)) (t 0)))

(defun assigns ()                                             ; *axes* 笛卡爾積 → env alist 清單
  (labels ((rec (ax) (if (null ax) (list '())
                         (loop for rest in (rec (cdr ax)) append
                               (loop for i below (cdr (car ax)) collect (acons (caar ax) i rest))))))
    (rec *axes*)))

(defun 成本 (母題 env intent history)                         ; 綁 env→組場→護欄剪枝→prior＋intent＋history
  (let* ((*env* (acons :母題 母題 env)) (s (rz '場)))
    (if (有非canon? s) nil                                    ; 護欄：非 canon＝不可行（剪枝）
        (values (+ (reduce #'+ env :key #'cdr :initial-value 0) ; prior：偏好前面候選
                   (icost intent s) (if (gethash s history) 100 0)) s))))

(defun direct (母題 intent &optional (history (make-hash-table :test 'equal)))
  (let (bs benv (bc most-positive-fixnum))                    ; 嚴格 <：先枚舉者贏平手
    (dolist (env (assigns))
      (multiple-value-bind (c s) (成本 母題 env intent history)
        (when (and c (< c bc)) (setf bc c bs s benv env))))
    (values bs benv)))

(defun direct-n (母題 intent n)                               ; N 種：每輪把用過的場加 history 逼換
  (let ((h (make-hash-table :test 'equal)) out)
    (dotimes (k n) (let ((s (direct 母題 intent h))) (push s out) (when s (setf (gethash s h) t))))
    (nreverse out)))
