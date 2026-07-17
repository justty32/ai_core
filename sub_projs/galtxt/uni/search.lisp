;;;; search.lisp — realizer 側搜尋：兩種耦合都建在同一支 search*（見 search_kernel.lisp）。
;;;; 可分：director 搜 *env* 風格軸（prior 累加、護欄/風格落終端）。
;;;; 耦合：搜耦合 示範全場預算（step 穿線、超支剪枝）＝gen_v0 fill_sequence 的形態。

(defparameter *axes* '((:move . 3) (:emo . 3)))               ; 可搜風格軸與定義域大小
(defparameter *非canon* '("咖啡廳" "陽溜亭" "天台" "教室"))
(defun 有非canon? (s) (some (lambda (w) (search w s)) *非canon*))
(defun icost (intent s)                                       ; 風格意圖成本
  (case intent (:甜 (if (search "……啊" s) 0 6))
               (:靜 (floor (length s) 8)) (t 0)))

;;; ── 可分案例：director＝search* 搜風格軸 ──
;;; points＝各軸的候選 (key . idx)；step 累加 prior(＝idx)；done 綁 env→組場→護欄剪枝→intent＋history
(defun direct (母題 intent &optional (history (make-hash-table :test 'equal)))
  (let ((chosen (search* (mapcar (lambda (a) (loop for i below (cdr a) collect (cons (car a) i))) *axes*)
                         (lambda (st c) (values (cons c st) (cdr c)))
                         (lambda (st chosen) (declare (ignore st))
                           (let* ((*env* (acons :母題 母題 chosen)) (s (rz '場)))
                             (if (有非canon? s) nil (+ (icost intent s) (if (gethash s history) 100 0))))))))
    (if chosen (values (let ((*env* (acons :母題 母題 chosen))) (rz '場)) chosen) (values nil nil))))

(defun 成本 (母題 env intent history)                         ; demo⑥ 用：單一 env 的成本(nil＝護欄剪掉)
  (let* ((*env* (acons :母題 母題 env)) (s (rz '場)))
    (if (有非canon? s) nil (+ (reduce #'+ env :key #'cdr :initial-value 0)
                              (icost intent s) (if (gethash s history) 100 0)))))

(defun direct-n (母題 intent n)                               ; N 種：每輪把用過的場加 history 逼換
  (let ((h (make-hash-table :test 'equal)) out)
    (dotimes (k n) (let ((s (direct 母題 intent h))) (push s out) (when s (setf (gethash s h) t))))
    (nreverse out)))

;;; ── 耦合案例：全場「……」預算 ≤ cap（跨節點約束，flat enum 表達不了、需 step 穿線）──
(defparameter *beats*                                         ; 每 beat 候選：(台詞 . 含「……」數)
  (list (list (cons "秋穗：「……會怕。」" 1) (cons "秋穗：「會怕。」" 0))
        (list (cons "主角：「……別怕。」" 1) (cons "主角：「別怕。」" 0))
        (list (cons "秋穗：「……嗯。」" 1) (cons "秋穗：「嗯。」" 0))))
(defun 搜耦合 (cap)                                           ; 用同一支 search*；step 穿線預算、超支剪枝
  (search* *beats*
           (lambda (spent c) (let ((s (+ spent (cdr c)))) (if (> s cap) (values nil 0) (values s 0))))
           (lambda (spent chosen) (declare (ignore spent))    ; 偏好「……」多＝成本低（缺者計 1）
             (reduce #'+ chosen :key (lambda (c) (if (search "……" (car c)) 0 1))))
           0))
