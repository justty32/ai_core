;;;; main.lisp — realizer_l 跑批：load 核心＋director，跑全部 demo（自帶 assert）。
;;;; 跑：sbcl --script main.lisp
(load "realizer.lisp") (load "director.lisp")
(defun hr (s) (format t "~%========== ~a ==========~%" s))

;;; ── realizer 五 demo（原⑥「換座標」由 director 的 intent 取代，見 ⑧）──
(let ((s1 (scene-text "櫻花")))
  (hr "① 全場生成：母題＝櫻花，缺省座標") (format t "~a~%" s1)
  (assert (search "河堤" s1)) (assert (search "沒關係的。就算櫻花變了" s1)) (assert (search "走吧" s1))
  (hr "② 確定性自檢") (assert (string= s1 (scene-text "櫻花"))) (format t "逐位元相同 ✓~%")
  (hr "③ 多模板：段 D 三種句型")
  (let ((ds (enum-d1 "櫻花")) (seen (make-hash-table :test 'equal)))
    (dolist (e ds) (format t "  [~a｜~a] ~a~%" (first e) (second e) (third e)) (setf (gethash (third e) seen) t))
    (assert (>= (hash-table-count seen) 3)))
  (hr "④ 護欄：變項×動詞（季節×謝了）")
  (multiple-value-bind (txt why) (line (first (al "d1" *tmpl*)) "季節" '(("變化動詞" . 2)))
    (assert (null txt)) (format t "被擋 ✓：~a~%" why))
  (hr "⑤ 護欄：收束（錨引入咖啡廳）")
  (let ((st (make-hash-table :test 'equal)))
    (setf (gethash "不變的錨" st) "我們改天去咖啡廳坐坐")
    (multiple-value-bind (ok why) (ck st "櫻花") (assert (null ok)) (format t "被擋 ✓：~a~%" why))))

;;; ── director 四 demo：搜索升格 ──
(hr "⑦ director：搜最優（母題=櫻花，無 intent）＝缺省稿")
(multiple-value-bind (s tr) (direct "櫻花" nil)
  (format t "~a~%選用模板：~{~a ~}~%" s tr)
  (assert (string= s (scene-text "櫻花"))))   ; 無 intent → 全取首模板/首候選 ＝ 缺省

(hr "⑧ director：intent 甜 vs 靜 → 搜出不同風格（取代舊「換座標」）")
(let ((甜 (direct "櫻花" :甜)) (靜 (direct "櫻花" :靜)))
  (format t "【甜】~%~a~%~%【靜】~%~a~%" 甜 靜)
  (assert (not (string= 甜 靜))))

(hr "⑨ director：direct-n → 自動生 3 種遞減優選台詞流（呼應最初手寫「20 種」）")
(let ((ss (direct-n "櫻花" nil 3)) (seen (make-hash-table :test 'equal)))
  (loop for s in ss for i from 1 do (format t "── 第 ~a 種 ──~%~a~%~%" i s) (setf (gethash s seen) t))
  (assert (= (hash-table-count seen) 3)))

(format t "~%全部 demo 通過——director（搜索）＋realizer（Common Lisp）立住了。~%")
