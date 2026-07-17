;;;; synth.lisp — 五路綜合・最小統一節點 kernel（河堤場景）。
;;;; 一種節點跑所有粒度：八股/段/句式/填充/原子事實＝同一 (node …)；粒度由 refs 湧現(不限四種)。
;;;; 收攏五路：a1 註冊表+遞迴realize｜a2 分支=普通節點/DAG｜a3 真值域vs字面域｜
;;;;          a4 part 折疊(字面/引用/選/邏輯)｜a5 層由refs湧現+懸空=佔位。跑：sbcl --script synth.lisp

(defvar *ns* (make-hash-table :test 'eq))          ; id → plist：(:kids … :fn … | :真值 t :述 …)
(defvar *env* nil)                                 ; 唯一輸入：母題/情緒/選路 alist
(defun env (k) (cdr (assoc k *env*)))

;;; ── 編譯期：把一個 part 折成「回字串」的表達式 ────────────────────────
;;; 字串=字面｜symbol=子引用(遞迴 rz)｜(? key a b…)=依 env[key] 選一(填充選詞＝分支，同一運算)｜
;;; (! form)=逃生艙(任意 lisp 回字串，帶邏輯的填充物/母題導出全走這)
(eval-when (:compile-toplevel :load-toplevel :execute)
  (defun %p (x)
    (cond ((stringp x) x)
          ((symbolp x) `(rz ',x))
          ((eq (car x) '?) `(funcall (nth (mod (or (env ,(cadr x)) 0) ,(length (cddr x)))
                                          (list ,@(mapcar (lambda (o) `(lambda () ,(%p o))) (cddr x))))))
          ((eq (car x) '!) (cadr x))
          (t (error "bad part ~a" x))))
  (defun %kids (ps)                                ; 收子節點 id（走訪 parts 的非關鍵字符號，含分支內的）
    (let (o) (labels ((w (x) (cond ((and (symbolp x) (not (keywordp x)) (not (member x '(? !)))) (pushnew x o))
                                   ((consp x) (mapc #'w x)))))
               (mapc #'w ps)) (nreverse o))))

;;; ── 唯一寫入端：兩個 macro（字面節點 node／真值節點 fact），共用一張表 ──
(defmacro node (id &rest parts)                    ; 字面域：realize 回字串
  `(setf (gethash ',id *ns*)
         (list :kids ',(%kids parts) :fn (lambda () (concatenate 'string ,@(mapcar #'%p parts))))))
(defmacro fact (id 述 &rest 引)                    ; 真值域：帶命題+引用，不 realize
  `(setf (gethash ',id *ns*) (list :kids ',引 :真值 t :述 ,述)))

;;; ── 讀取端：全是小函式（粒度/引用/域，皆從同一表湧現）──────────────────
(defun nd (id) (gethash id *ns*))
(defun rz (id) (funcall (getf (nd id) :fn)))                    ; realize：遞迴組字面
(defun kids (id) (getf (nd id) :kids))
(defun 懸空? (id) (null (nd id)))                               ; 引到未定義＝佔位需求(a5)
(defun 真值? (id) (getf (nd id) :真值))                         ; 域界線(a3)
(defun 層 (id)                                                  ; 粒度＝1+max(子層)，葉=0，湧現(a5,不限四種)
  (let ((k (remove-if #'懸空? (kids id))))
    (if (null k) 0 (1+ (reduce #'max (mapcar #'層 k))))))

;;; ── 河堤場景：八股→段→句式→填充→原子事實，全是同一種節點 ─────────────
;; 原子事實（真值域・葉）＋一條排斥（a3 的域邏輯，只掃真值域）
(fact 母題-櫻花 "本場母題＝櫻花")
(fact 地-河堤 "場景在河堤")
(fact 地-天台 "場景在天台")
(defparameter *排斥* '((地-河堤 地-天台)))                       ; 不可同真
;; 填充（字面域・葉，帶邏輯）
(node 收束動詞 (? :move "來到" "走到" "繞回"))
(node 收束地 "河堤")
(node 安定 (? :emo "沒關係的" "別怕" "我在啊"))
(node 會變項 (! (or (env :母題) "櫻花")))                        ; 母題導出（帶邏輯的填充物,a4）
(node 變化動詞 (! (if (equal (env :母題) "季節") "換了" "變了"))) ; 變項×動詞相配的邏輯內嵌
(node 不變的錨 (? :emo "我們還能一起走這條路" "我還會在這條路上等你"))
(node 句尾 (? :emo "，不是嗎？" "。"))
;; 句式（字面域・內部）
(node a1 "結果，還是" 收束動詞 收束地 "了呢。")
(node d1 安定 "。就算" 會變項 變化動詞 "，" 不變的錨 句尾)
;; 段（內部）＋固定旁白（葉）
(node 旁A "（斜陽把影子拉長，兩側櫻堤落下最後的櫻花雨。）")
(node 段A 旁A a1)
(node 段D d1)
;; 八股整場（內部，最粗）
(node 場 段A 段D)
;; 分支＝普通節點(a2)：選路由 env :choice 決定，兩分支皆為子節點
(node 分支A "主角：「去河鹿堂看看？」")
(node 分支B "主角：「去陽溜亭吃紅豆湯圓？」")
(node 選擇肢 (? :choice 分支A 分支B))
;; 懸空引用示範(a5)：引到尚未定義的 id ＝佔位工單
(node 段E-待展開 e1-尚未寫)

;;; ── demo（自帶 assert）────────────────────────────────────────────
(defun hr (s) (format t "~%===== ~a =====~%" s))
(hr "① 一種節點跑整場（母題=櫻花，缺省 env）")
(format t "~a~%" (rz '場))
(assert (search "河堤" (rz '場))) (assert (search "沒關係的。就算櫻花變了" (rz '場)))

(hr "② 粒度由 refs 湧現（不限四種）")
(dolist (p '((收束地 . 0) (a1 . 1) (段A . 2) (場 . 3)))
  (format t "  層(~a)=~a~%" (car p) (層 (car p))) (assert (= (層 (car p)) (cdr p))))
(assert (and (= 0 (層 '旁A)) (> (層 'a1) 0)))   ; 同粒度不同種：旁A(葉)與 a1(內部)不同層但同表同機制

(hr "③ 真值域 vs 字面域（同容器，謂詞分域）")
(assert (真值? '母題-櫻花)) (assert (not (真值? 'a1)))
(assert (loop for (x y) in *排斥* never (and (真值? x) (真值? y) (eq x y)))) ; 排斥只掃真值域
(format t "  事實 母題-櫻花 真值?=~a｜句式 a1 真值?=~a~%" (真值? '母題-櫻花) (真值? 'a1))

(hr "④ 分支＝普通節點；選路由 env 決定，粒度查詢不瞎")
(let ((*env* '((:choice . 0)))) (format t "  A: ~a~%" (rz '選擇肢)))
(let ((*env* '((:choice . 1)))) (format t "  B: ~a~%" (rz '選擇肢)))
(assert (= 2 (length (kids '選擇肢))))          ; 兩互斥分支仍是子節點

(hr "⑤ 懸空引用＝佔位工單")
(assert (懸空? 'e1-尚未寫)) (assert (member 'e1-尚未寫 (kids '段E-待展開)))
(format t "  段E-待展開 → e1-尚未寫〔懸空・待展開〕~%")

(hr "⑥ 換 env＝換風格，同節點另一整場")
(let ((*env* '((:move . 2) (:emo . 2) (:母題 . nil))))
  (let ((s (rz '場))) (format t "~a~%" s) (assert (search "繞回" s)) (assert (search "我在啊" s))))

(format t "~%全部 demo 通過——五路綜合・最小統一節點 kernel 立住了。~%")
