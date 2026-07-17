;;;; uniform.lisp — 統一粒度節點 kernel（Common Lisp）。
;;;; 洞見：八股／段落／句式／填充物／原子事實 不是四種東西，只是同一種「節點」的不同粒度。
;;;;   葉節點帶值、可帶邏輯判斷；內部節點帶子節點引用＋組合／護欄／成本邏輯；粒度任意深。
;;;; 一個 macro `n` 折出所有粒度；把《最後的櫻花雪》場景一（舊校舍天台開場）
;;;;   從最粗（整場八股）到最細（原子事實／填充物）全用同一種節點表示。
;;;; 跑：sbcl --script uniform.lisp（自帶 assert：組出整場、逐位元確定、粒度可查）。

;;; ══ kernel ══════════════════════════════════════════════════════════
;;; 節點表：id(symbol) → (kids . fn)。fn:(env)->string；kids 只為「查粒度」而存。
(defvar *ns* (make-hash-table :test 'eq))
(defun rz (id env)                                   ; realize：查表、遞迴組出字串
  (funcall (cdr (or (gethash id *ns*) (error "無此節點：~a" id))) env))
(defun g* (env k &optional d) (getf env k d))        ; env 取值（分支選擇／填充索引…）
(defun pick (env k cands) (nth (1- (g* env k 1)) cands)) ; 葉的候選：env 指第幾個、預設 1

;;; 唯一 kernel macro `n`：一串 parts 折成一個節點（同一 macro 蓋所有粒度）。
;;;   part：字串=字面；keyword=換行(:br)；bare symbol=子節點引用(遞迴 rz)；
;;;        (form)=邏輯判斷（env、rz 可見，須回字串；護欄可在此 assert，回 "" 不佔字）。
;;;   kids（查粒度用）＝bare-symbol 子節點 ＋ form 內 'quote 到的子節點（動態分支也算）。
(eval-when (:compile-toplevel :load-toplevel :execute)
  (defun %kids (parts)                               ; 掃 parts 收子節點 id
    (let (ks)
      (labels ((q (x) (cond ((and (consp x) (eq (car x) 'quote) (symbolp (cadr x))) (pushnew (cadr x) ks))
                            ((consp x) (q (car x)) (q (cdr x))))))
        (dolist (p parts)
          (cond ((keywordp p)) ((and (symbolp p) p) (pushnew p ks)) ((consp p) (q p)))))
      (nreverse ks)))
  (defun %part (p)                                   ; 一個 part 折成 concatenate 的一段
    (cond ((stringp p) p) ((keywordp p) (string #\Newline))
          ((symbolp p) `(rz ',p env)) (t p))))
(defmacro n (id &rest parts)
  `(setf (gethash ',id *ns*)
         (cons ',(%kids parts)
               (lambda (env) (declare (ignorable env))
                 (concatenate 'string ,@(mapcar #'%part parts))))))

;;; 粒度查詢：葉／子／深度／可達集（DAG 去重）。
(defun kids-of (id) (car (gethash id *ns*)))
(defun leafp (id) (null (kids-of id)))
(defun depth (id) (if (leafp id) 0 (1+ (reduce #'max (mapcar #'depth (kids-of id)) :initial-value 0))))
(defun subtree (id &optional acc)
  (if (member id acc) acc
      (let ((acc (cons id acc))) (dolist (k (kids-of id) acc) (setf acc (subtree k acc))))))
(defun csum (s)                                      ; 逐位元確定性校驗和（凍結後可斷言）
  (let ((h 0)) (loop for c across s for i from 1 do (setf h (mod (+ (* h 131) (char-code c) i) 1000000007))) h))

;;; ══ 場景一・舊校舍天台開場（素材擷自 劇本.md，全用 `n` 折成統一節點）══════
;;; L4 原子事實／填充物（葉：帶值、或帶邏輯判斷）───────────────────────────
(n 安靜詞 (pick env :安靜 '("最安靜" "最清靜")))       ; 葉帶邏輯：候選，env 指定、預設1=canon
(n 町名 "日和町") (n 河名 "霞川")                       ; 河名＝共享原子，開場與邀約都引用（DAG）
(n 秋穗身分 "新生代表")                                 ; canon 事實（身分交代的最小單位）
(n 舊書描述 "一本泛黃的舊書") (n 書店名 "河鹿堂") (n 印章 "梅花章") ; 分支伏筆物的原子
(n 未來點 "待會放學")                                   ; 邀約引子指向的未來時間點

;;; L3 句式（內部節點：把原子縫成一個子句）────────────────────────────────
(n 舊書句 "（她轉過身，手裡拿著" 舊書描述 "，封面上印著老書店『" 書店名 "』的" 印章 "。）")

;;; L2 台詞（旁白＝裸字面；對白＝「角色：『…』」；有的引用句式／原子，有的是純字面葉）──
;; ① 開場空鏡
(n 空鏡1 "（四月的風夾雜著櫻花瓣的餘香，掠過空曠的鐵絲網。）")
(n 空鏡2 "（牆角堆放著廢棄的課桌椅，被陽光曬得微微發熱，散發出陳舊的木頭氣味。）")
(n 空鏡3 "（鏽蝕的圍欄外，可以俯瞰整座" 町名 "——靜靜流淌的" 河名 "，以及河畔兩側漸漸由粉轉綠的櫻並木。）")
(n 主角問安 "主角：「呼……果然這裡" 安靜詞 "了。」")
;; ② 主角登場
(n 登場旁1 "（推開那扇從不上鎖的鐵門，本以為只有自己，卻看見一個熟悉的背影立在護欄前。）")
(n 登場旁2 "（那是同班的秋穗。她正微微踮起腳尖，任憑風吹亂她黑色的短髮。）")
(n 秋穗招呼 "秋穗：「啊，是你。你也來這裡躲避開學典禮的吵鬧嗎？」")
(n 主角回應 "主角：「嗯，禮堂裡實在太悶了。話說回來，你不用去代表新生致詞？」")
;; ③ 相認・交代身分
(n 秋穗致詞畢 "秋穗：「早就結束啦。況且，比起聽校長冗長的訓話，我更想看今年的最後一場櫻花雪。」")
(n 主角點期待 "主角：「畢竟是" 秋穗身分 "，校長肯定對你抱有很大期待吧。」")
(n 秋穗逃跑 "秋穗：「所以才想逃跑啊。比起那些期待，我更喜歡這種沒人打擾的日常。」")
;; ④ 舊書・拋出邀約引子（最深路徑：台詞→句式→原子）
(n 邀約旁白 舊書句)
(n 秋穗指河 "秋穗：「吶，你看。從這裡望下去，" 河名 "像一條綠色的帶子呢。」")
(n 主角嘆季 "主角：「確實。再過幾天，櫻花落盡，夏天就要來了吧。」")
(n 秋穗拋引 "秋穗：「是啊……時間過得真快。對了，" 未來點 "，你打算直接回家嗎？」")
;; 選項Ａ後續（河鹿堂）
(n A邀書 "主角：「如果沒事的話，要不要去『河鹿堂』看看？我看你手上的書，好像是那裡的。」")
(n A秋穗還書 "秋穗：「被你發現了？這本小說我剛好看到最後一章，正想去還呢。」")
(n A旁笑 "（她露出淡淡的微笑，將書抱在胸前，眼神亮了起來。）")
(n A秋穗推書 "秋穗：「那走吧，聽說老闆最近進了一批有趣的二手推理小說。」")
(n A旁下山 "（我們沿著杜坂並肩走下山，午後的陽光穿過樹蔭，在她的肩頭灑下斑駁光影。）")
;; 選項Ｂ後續（陽溜亭）
(n B邀食 "主角：「肚子有點餓了，要不要去『陽溜亭』吃個下午茶？今天的特餐好像是紅豆湯圓。」")
(n B秋穗誘 "秋穗：「欸？聽起來真誘人。正好我也覺得肚子空空的。」")
(n B旁紅 "（她微微偏過頭，臉頰似乎因為風吹而有些泛紅。）")
(n B秋穗爭 "秋穗：「那可不能落後了，要是被其他人搶先點完，我可是會生氣的。」")
(n B旁下山 "（我們沿著杜坂並肩走下山，空氣裡似乎也多了些甜甜的、讓人放鬆的溫度。）")

;;; L1 段落（八股的「格」；把台詞用 :br 換行縫起）─────────────────────────
(n 開場空鏡 空鏡1 :br 空鏡2 :br 空鏡3 :br 主角問安)
(n 主角登場 登場旁1 :br 登場旁2 :br 秋穗招呼 :br 主角回應)
(n 相認交身分 秋穗致詞畢 :br 主角點期待 :br 秋穗逃跑)
(n 邀約引子 邀約旁白 :br 秋穗指河 :br 主角嘆季 :br 秋穗拋引)
(n 分支A A邀書 :br A秋穗還書 :br A旁笑 :br A秋穗推書 :br A旁下山)
(n 分支B B邀食 :br B秋穗誘 :br B旁紅 :br B秋穗爭 :br B旁下山)

;;; 選擇肢＝四分法沒有的東西：一個「引用兩個互斥子節點、依 env 選擇實現」的節點。
(n 選擇肢 (if (eq (g* env :choice :A) :A) (rz '分支A env) (rz '分支B env)))

;;; L0 整場八股（最粗粒度，仍是同一種節點）────────────────────────────────
(n 天台場景 開場空鏡 :br 主角登場 :br 相認交身分 :br 邀約引子 :br 選擇肢)

;;; ══ 自帶 assert ═════════════════════════════════════════════════════
(defparameter *A* (rz '天台場景 '(:choice :A)))
(defparameter *B* (rz '天台場景 '(:choice :B)))
;;; ① 能組出整場台詞
(assert (plusp (length *A*)))
(assert (= 20 (1+ (count #\Newline *A*))))            ; 選項Ａ線＝20 行台詞
(assert (search "新生代表" *A*))                       ; 身分交代（③段）
(assert (search "待會放學" *A*))                       ; 邀約引子（④段）
(assert (and (search "河鹿堂" *A*) (search "梅花章" *A*))) ; 分支伏筆物
;;; ② 確定性逐位元
(assert (string= *A* (rz '天台場景 '(:choice :A))))    ; 重算 bit-identical（零 RNG/零 hash 序依賴）
(assert (= 190086330 (csum *A*)))                      ; 凍結校驗和：byte-level 錨定
(assert (= 33518063 (csum *B*)))
(assert (and (search "二手推理小說" *A*) (not (search "二手推理小說" *B*)))) ; 分支Ａ獨有
(assert (and (search "紅豆湯圓" *B*) (not (search "紅豆湯圓" *A*))))         ; 分支Ｂ獨有
(assert (and (search "所以才想逃跑啊" *A*) (search "所以才想逃跑啊" *B*)))   ; 共有段一字不差
;;; ③ 粒度可查（同一種節點、粒度任意深）
(assert (= 4 (depth '天台場景)))                        ; 深度 0..4 ＝ 5 個粒度層（不限四種）
(assert (leafp '河名))                                  ; 原子事實＝葉
(assert (not (leafp '天台場景)))                        ; 八股＝內部節點
(assert (member '河名 (subtree '開場空鏡)))             ; 同一原子被兩段共享引用（DAG）
(assert (member '河名 (subtree '邀約引子)))
(assert (= 2 (length (kids-of '選擇肢))))               ; 選擇肢＝引用兩互斥分支的節點

;;; ══ 報表 ════════════════════════════════════════════════════════════
(format t "════ 天台場景（選項Ａ・河鹿堂）════~%~a~%~%" *A*)
(format t "── 粒度分佈（同一種節點，depth＝到最深葉的結構層數；粒度不限四種）──~%")
(let ((cnt (make-hash-table)) (eg (make-hash-table)))
  (dolist (id (subtree '天台場景)) (incf (gethash (depth id) cnt 0)) (setf (gethash (depth id) eg) id))
  (loop for d from 0 to (depth '天台場景)
        do (format t "  depth ~a：~2a 個節點（例：~a）~%" d (gethash d cnt 0) (gethash d eg))))
(format t "── 選擇肢分支：Ａ與Ｂ僅『事・物』段不同，人時地共有段逐字相同 ──~%")
(format t "  |A|=~a |B|=~a csum(A)=~a csum(B)=~a~%" (length *A*) (length *B*) (csum *A*) (csum *B*))
(format t "全部 assert 通過。~%")
