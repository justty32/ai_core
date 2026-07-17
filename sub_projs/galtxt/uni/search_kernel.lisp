;;;; search_kernel.lisp — 一支參數化 search（DFS＋下界剪枝＋狀態穿線）。兩種耦合都跑。
;;;; 「一台搜尋機」的最後一里：gen_v0 fill_sequence 與 uni director 收斂成同一支函式。
;;;;
;;;; points : 每個選擇點的候選清單（list of list）
;;;; step   : (state cand) → (values new-state add-cost)；new-state=nil ⇒ 不可行(剪枝)
;;;; done   : (state chosen) → 終端成本 或 nil(不可行)
;;;; 回      (values 最優chosen 最優cost)。確定性：固定枚舉序、嚴格 < 先枚舉者贏平手。
;;;;
;;;; 耦合案例（gen_v0）：step 穿線全場預算、超支回 nil 剪枝。
;;;; 可分案例（uni director）：step 純累加 prior、護欄/風格成本落在 done（終端）。

(defun search* (points step done &optional init)
  (let (best (bc most-positive-fixnum))
    (labels ((dfs (ps st acc chosen)
               (when (< acc bc)                              ; 下界剪枝（step 成本非負增量）
                 (if ps
                     (dolist (cand (car ps))
                       (multiple-value-bind (nst add) (funcall step st cand)
                         (when nst (dfs (cdr ps) nst (+ acc add) (cons cand chosen)))))
                     (let ((rc (funcall done st (reverse chosen))))
                       (when (and rc (< (+ acc rc) bc))
                         (setf bc (+ acc rc) best (reverse chosen))))))))
      (dfs points init 0 '()))
    (values best bc)))
