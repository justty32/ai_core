;;;; a5.lisp — 統一節點 kernel。八股／段／句／填充物／原子事實＝同一種「節點」，
;;;;   粒度任意（不限四種、往上往下都同型）。本檔重點：用「比場景更粗」的節點，
;;;;   把《最後的櫻花雪》整篇劇本的高層戲劇結構搭起來——
;;;;   相遇（天台）→ 分支（河鹿堂／陽溜亭）→ 收束（河堤・兩線合一）→ 主旨落點 → 尾聲（車站）。
;;;;   高層節點「引用」下層場景／段／句節點；未展開者以「懸空引用＝佔位需求」表示（呼應 gen_v1）。
;;;;   跑：sbcl --script a5.lisp   自帶 assert。
;;; ══ kernel：唯一資料型「節點」════════════════════════════════════════════════
;;;   一切事實（不論粒度）都是 (node id …plist…)，存進 *facts*[id]。欄位皆選配：
;;;     :層   粒度標（自由，僅註記；真正「層數」由 refs 推導＝自然湧現，見 層數）
;;;     :述   一句話描述（骨架印這個）
;;;     :引   refs：指向的子節點 id 清單；指向未定義者＝懸空＝佔位需求
;;;     :順路 收束邊（分支線專用）：指向兩線該合一的節點——分支收束護欄靠它機械成立
;;;     :效   (lambda (w)…) 進入節點時對「跨場景世界狀態 w」的回寫（季節時鐘／情緒弧）
;;;     :護   (lambda (w)…) 節點內建的邏輯判斷／護欄，回 (values ok why)
;;;   內部節點可帶大量邏輯（:效 節奏推移、:護 結構決策／收束護欄），全塞進同一型。
(defvar *facts* (make-hash-table :test 'eq))
(defvar *order* nil)                              ; 註冊序 → 確定性遍歷
(defun reg (id p)
  (unless (nth-value 1 (gethash id *facts*)) (setf *order* (append *order* (list id))))
  (setf (gethash id *facts*) p) id)
(defmacro node (id &rest kv)                      ; ← kernel macro：一行把節點壓成 plist 註冊
  `(reg ',id (list ,@(loop for (k v) on kv by #'cddr append
                           (list k (if (member k '(:護 :效))
                                       `(lambda (w) (declare (ignorable w)) ,v)
                                       `',v))))))
;;; ── 存取／派生 ─────────────────────────────────────────────────────────────
(defun 取 (id k &optional d) (getf (gethash id *facts*) k d))
(defun 懸空? (id) (not (nth-value 1 (gethash id *facts*))))          ; 未定義＝佔位需求
(defun 全序 () *order*)
(defun 層數 (id)                                   ; 層數自然湧現：葉／懸空＝0，內部＝子最大＋1
  (let ((c (取 id :引)))
    (if (or (null c) (懸空? id)) 0 (1+ (reduce #'max (mapcar #'層數 c) :initial-value 0)))))
(defun 接地? (id)                                  ; 沿 :引 下行能達「已定義的葉」＝接地；全懸空＝未接地
  (cond ((懸空? id) nil) ((null (取 id :引)) t) (t (some #'接地? (取 id :引)))))
;;; ── 跨場景世界狀態 w（母題時鐘／情緒弧作為可追蹤的歷史）─────────────────────
(defun 推 (w k v) (setf (gethash k w) (nconc (gethash k w) (list v))) v)
(defun 史 (w k) (gethash k w))
(defun 皆同? (l) (or (null l) (every (lambda (x) (eq x (car l))) l)))
(defun 遞增序? (值s 序)                            ; 值s 依 序 的索引須存在且不遞減（時鐘不倒退）
  (let ((is (mapcar (lambda (v) (position v 序)) 值s)))
    (and (every #'identity is) (or (null (cdr is)) (apply #'<= is)))))
;;; ── 走訪：骨架列印 ＋ 前序過帶（跑 :效 疊世界狀態）─────────────────────────
(defun 骨架 (id &optional (d 0))
  (unless (懸空? id)
    (format t "~a~a〔~a／層~a〕~a~%" (make-string (* 2 d) :initial-element #\　)
            id (or (取 id :層) '-) (層數 id) (or (取 id :述) ""))
    (dolist (c (取 id :引))
      (if (懸空? c)
          (format t "~a↳ ~a 〔懸空・佔位需求〕~%" (make-string (* 2 (1+ d)) :initial-element #\　) c)
          (骨架 c (1+ d))))))
(defun 過帶 (id w seen)                            ; 前序 DFS 跑 :效；確定性（:引 序）
  (when (and (not (懸空? id)) (not (gethash id seen)))
    (setf (gethash id seen) t)
    (let ((e (取 id :效))) (when e (funcall e w)))
    (dolist (c (取 id :引)) (過帶 c w seen))))
(defun 驗護 (w)                                    ; 全序跑每節點 :護 → (id ok why)
  (loop for id in (全序) for f = (取 id :護) when f
        collect (multiple-value-bind (ok why) (funcall f w) (list id ok why))))
(defun 佔位單 ()                                   ; 收集所有懸空引用＝待展開的下層節點工單
  (let (out) (dolist (id (全序)) (dolist (c (取 id :引))
                                  (when (懸空? c) (pushnew (list id c) out :test #'equal))))
       (nreverse out)))
;;; ══ 全劇高層結構（比場景更粗的節點；引用下層，多數懸空＝尚未展開）══════════════
(node 全劇 :層 全劇 :述 "最後的櫻花雪：相遇→分支→收束→主旨→尾聲"
      :引 (相遇 分支 收束 主旨 尾聲))
(node 相遇 :層 幕 :述 "天台相遇：自在起點・櫻花將盡・埋分支伏筆（河鹿堂舊書）"
      :引 (天台・開場 天台・相認 天台・舊書引子)                       ; ← 三個下層段節點皆懸空
      :效 (progn (推 w :情緒 '自在) (推 w :季 '將盡)))

(node 分支 :層 幕 :述 "邀約分支：只改『事・物』，人時地同源"
      :引 (線A 線B)
      ;; 分支收束護欄：A/B 皆須帶 :順路 邊、且合一到同一收束節點（＝『回車站順路』的機械形態）
      :護 (let* ((cs (取 '分支 :引)) (ts (mapcar (lambda (c) (取 c :順路)) cs)))
            (cond ((some #'null ts) (values nil "有分支線缺 :順路 收束邊"))
                  ((not (皆同? ts)) (values nil "分支線未合一到同一節點"))
                  ((懸空? (car ts)) (values nil "收束目標懸空"))
                  (t (values t (format nil "A/B 皆因『回車站順路』合一於「~a」" (car ts)))))))

(node 線A :層 場景 :述 "河鹿堂線（還書）" :順路 收束
      :引 (河鹿堂・邀約 河鹿堂・下山) :效 (推 w :去處 '河鹿堂))          ; ← 懸空
(node 線B :層 場景 :述 "陽溜亭線（下午茶）" :順路 收束
      :引 (陽溜亭・邀約 陽溜亭・下山) :效 (推 w :去處 '陽溜亭))          ; ← 懸空

(node 收束 :層 幕 :述 "霞川河堤：兩線合一・情緒低點（落寞）・落最後的櫻花雨"
      :引 (河堤・空鏡 河堤・談心)                                      ; 空鏡懸空、談心接地（下見）
      :效 (progn (推 w :情緒 '落寞) (推 w :季 '落雨)))

(node 主旨 :層 幕 :述 "主旨落點：風景會變，人還能一起走這條路（釋懷）"
      :引 (河堤・寬慰)                                                ; ← 懸空
      :效 (推 w :情緒 '釋懷)
      ;; 情緒弧護欄：跨場景累積的情緒史須為 自在→落寞→釋懷
      :護 (if (equal (史 w :情緒) '(自在 落寞 釋懷))
              (values t (format nil "情緒弧成立：~a" (史 w :情緒)))
              (values nil (format nil "情緒弧不符：~a" (史 w :情緒)))))

(node 尾聲 :層 幕 :述 "木造車站：櫻花落盡・落她髮梢・並肩歸途"
      :引 (車站・歸途)                                                ; ← 懸空
      :效 (推 w :季 '落盡)
      ;; 季節母題時鐘護欄：季史須沿 將盡→落雨→落盡 遞進、不倒退
      :護 (if (遞增序? (史 w :季) '(將盡 落雨 落盡))
              (values t (format nil "季節時鐘遞進：~a" (史 w :季)))
              (values nil (format nil "季節時鐘倒退：~a" (史 w :季)))))

;;; ── 接地示範：同一種節點一路往下疊到「句」層（往下也不限，接地不變式）──────────
(node 河堤・談心 :層 場景 :述 "談心段" :引 (段・吐露不安))
(node 段・吐露不安 :層 段 :述 "秋穗吐露：怕停在原地、怕熟悉風景變樣" :引 (句・怕變樣))
(node 句・怕變樣 :層 句 :述 "「害怕那些熟悉的風景，不知不覺就變了樣。」")     ; ← 定義的葉＝層0

;;; ══ 執行＋自帶 assert ════════════════════════════════════════════════════════
(format t "~&═══ 高層戲劇結構骨架 ═══~%")
(骨架 '全劇)

(let ((w (make-hash-table :test 'eq)))
  (過帶 '全劇 w (make-hash-table :test 'eq))
  (format t "~%═══ 跨場景狀態（母題時鐘／情緒弧）═══~%")
  (format t "  情緒弧：~a~%  季節鐘：~a~%  分支去處：~a~%"
          (史 w :情緒) (史 w :季) (史 w :去處))

  (format t "~%═══ 節點護欄 ═══~%")
  (dolist (r (驗護 w)) (format t "  ~:[✗~;✔~] ~a：~a~%" (second r) (first r) (third r)))

  (format t "~%═══ 懸空引用＝佔位需求（待下層展開的工單）═══~%")
  (dolist (p (佔位單)) (format t "  ~a → ~a~%" (first p) (second p)))

  ;; ── assert 群 ──
  (format t "~%═══ assert ═══~%")
  ;; ① 骨架可整走：全劇接地（沿 refs 下行達已定義的葉）
  (assert (接地? '全劇)) (format t "  ✔ 高層結構接地（達句層葉節點）~%")
  ;; ② 層數自然湧現且高層最粗：全劇 > 收束 > 河堤・談心 > 句
  (assert (> (層數 '全劇) (層數 '收束) (層數 '河堤・談心) (層數 '句・怕變樣)))
  (format t "  ✔ 層數自然湧現：全劇~a > 收束~a > 談心~a > 句~a~%"
          (層數 '全劇) (層數 '收束) (層數 '河堤・談心) (層數 '句・怕變樣))
  ;; ③ 分支收束護欄成立：A/B :順路 皆指向 收束
  (assert (皆同? (list (取 '線A :順路) (取 '線B :順路) '收束)))
  (assert (second (assoc '分支 (驗護 w))))
  (format t "  ✔ 分支收束護欄：線A／線B 皆因『回車站順路』合一於 收束~%")
  ;; ④ 情緒弧 自在→落寞→釋懷
  (assert (equal (史 w :情緒) '(自在 落寞 釋懷)))
  (format t "  ✔ 情緒弧：自在→落寞→釋懷~%")
  ;; ⑤ 季節母題時鐘遞進不倒退
  (assert (遞增序? (史 w :季) '(將盡 落雨 落盡)))
  (format t "  ✔ 季節時鐘：將盡→落雨→落盡~%")
  ;; ⑥ 懸空引用可標記：佔位單非空、且每筆確為未定義節點
  (assert (佔位單)) (assert (every (lambda (p) (懸空? (second p))) (佔位單)))
  (format t "  ✔ 懸空引用可標記：~a 筆佔位需求~%" (length (佔位單)))
  ;; ⑦ 全部護欄綠
  (assert (every #'second (驗護 w)))
  (format t "  ✔ 全部節點護欄通過~%")
  (format t "~%── 全綠：高層戲劇結構用同一種節點成立 ──~%"))
