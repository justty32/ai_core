;;;; main.lisp — uni 跑批：load kernel→scene→search，跑全部 demo（自帶 assert）。
;;;; 跑：sbcl --script main.lisp
(load "kernel.lisp") (load "scene.lisp") (load "search_kernel.lisp") (load "search.lisp")
(defun hr (s) (format t "~%===== ~a =====~%" s))

;;; ── 表示：一種節點跑所有粒度 ──
(hr "① 一種節點跑整場（母題=櫻花，缺省 env）")
(let ((*env* '((:母題 . "櫻花")))) (let ((s (rz '場))) (format t "~a~%" s)
  (assert (search "河堤" s)) (assert (search "沒關係的。就算櫻花變了" s)) (assert (search "走吧" s))))

(hr "② 粒度由 refs 湧現（不限四種）")
(dolist (p '((收束地-無.0) (a1 . 1) (段A . 2) (場 . 3)))
  (when (nd (car p)) (format t "  層(~a)=~a~%" (car p) (層 (car p)))))
(assert (= 0 (層 '旁A))) (assert (= 1 (層 'a1))) (assert (= 3 (層 '場)))
(assert (and (= 0 (層 '旁A)) (> (層 'a1) 0)))          ; 同表：旁A(葉)與 a1(內部)不同層同機制

(hr "③ 真值域 vs 字面域（同容器，謂詞分域）")
(assert (真值? '母題-櫻花)) (assert (not (真值? 'a1)))
(assert (loop for (x y) in *排斥* always (and (真值? x) (真值? y))))  ; 排斥只掃真值域
(format t "  事實 母題-櫻花 真值?=~a｜句式 a1 真值?=~a~%" (真值? '母題-櫻花) (真值? 'a1))

;;; ── 演算法：共用 search kernel 搜 *env* ──
(hr "④ director：搜最優（母題=櫻花，無 intent）")
(multiple-value-bind (s e) (direct "櫻花" nil)
  (format t "~a~%env=~a~%" s e) (assert (not (有非canon? s))))

(hr "⑤ intent 甜 vs 靜 → 搜出不同風格")
(let ((甜 (direct "櫻花" :甜)) (靜 (direct "櫻花" :靜)))
  (format t "【甜】~a~%~%【靜】~a~%" 甜 靜) (assert (not (string= 甜 靜))))

(hr "⑥ 護欄剪枝：非 canon 被搜索排除")
(let* ((e '((:emo . 2) (:move . 0))) (raw (let ((*env* (acons :母題 "櫻花" e))) (rz '場))))
  (assert (有非canon? raw)) (assert (null (成本 "櫻花" e nil (make-hash-table :test 'equal))))
  (format t "  :emo=2 生出含『咖啡廳』的場 → 成本=nil 被剪枝 ✓~%"))

(hr "⑦ direct-n → 自動生 3 種台詞流（呼應手寫「20 種」）")
(let ((ss (direct-n "櫻花" nil 3)) (seen (make-hash-table :test 'equal)))
  (loop for s in ss for i from 1 do (format t "── 第 ~a 種 ──~%~a~%~%" i s) (setf (gethash s seen) t))
  (assert (= 3 (hash-table-count seen))))

;;; ── 分支/懸空 ──
(hr "⑧ 分支＝普通節點（選路由 env）")
(let ((*env* '((:choice . 0)))) (format t "  A: ~a~%" (rz '選擇肢)))
(let ((*env* '((:choice . 1)))) (format t "  B: ~a~%" (rz '選擇肢)))
(assert (= 2 (length (kids '選擇肢))))

(hr "⑨ 懸空引用＝佔位工單")
(assert (懸空? 'e1-尚未寫)) (assert (member 'e1-尚未寫 (kids '段E待展開)))
(format t "  段E待展開 → e1-尚未寫〔懸空・待展開〕~%")

;;; ── 最後一里：可分(director)與耦合(gen_v0 形態)跑同一支 search* ──
(hr "⑩ 同一支 search*：全場預算耦合案例（gen_v0 形態，flat enum 表達不了）")
(multiple-value-bind (c3 cost3) (搜耦合 3)
  (format t "  預算 ……≤3：~{~a~}（成本 ~a）~%" (mapcar #'car c3) cost3)
  (assert (= 0 cost3)))                                       ; 夠寬 → 三句全帶「……」
(multiple-value-bind (c1 cost1) (搜耦合 1)
  (format t "  預算 ……≤1：~{~a~}（成本 ~a）~%" (mapcar #'car c1) cost1)
  (assert (= 2 cost1))                                        ; 只准一句帶「……」→ 另兩句無、成本2
  (assert (= 1 (count-if (lambda (x) (search "……" (car x))) c1))))
(format t "  ↑ 同一支 search*：director＝可分(prior累加+終端護欄)，本例＝耦合(step 穿線預算+剪枝)~%")

(format t "~%全部 demo 通過——uni：統一節點 ＋ 一支 search*（可分/耦合皆跑）立住了。~%")
