;;;; uniform.lisp — 統一粒度節點 kernel。八股／段落／句式／填充物 塌成一條「粒度軸」。
;;;; 一個 macro (node) 折出所有層級：葉帶值＋邏輯、內部帶子節點引用＋護欄。
;;;; 零 LLM／零 RNG、確定性逐位元。跑：sbcl --script uniform.lisp

;;; ══ kernel：唯一的節點 macro ＋ 幾個查詢/組合原語 ═══════════════════════════
;;; 核心洞見：八股/段落/句式/填充物「不是不同種，只是不同粒度」。全部是同一種
;;; 「節點」＝(id level realize-λ)。內部節點在 body 裡用 (r "子") 引用子節點（可掛護欄／
;;; 成本邏輯）；葉節點的 body 直接求出字面（可含選擇／母題導出／可空等邏輯）。粒度＝軸上座標。
(defvar *ns*  (make-hash-table :test 'equal))   ; id → (id level realize-λ)
(defvar *env* nil)                              ; 風格座標／母題（alist，唯一的確定性輸入）
(defun k (id) (string-upcase (string id)))      ; 統一鍵：ASCII 折大寫、CJK 不動（合 reader）

(defmacro node (id level &body body)
  "統一粒度節點：把 id 綁到 (id level λ)。body 求值＝該節點實現出的字串。
   葉＝body 是字面/選擇/母題導出（可含邏輯判斷）；內部＝body 內用 (r \"子\") 引用子節點（可含護欄）。"
  `(setf (gethash ,(k id) *ns*) (list ,(k id) ,level (lambda () ,@body))))

(defun nd  (id) (or (gethash (k id) *ns*) (error "無此節點：~a" id)))
(defun lvl (id) (second (nd id)))                       ; ← 粒度層級可查（節點在軸上的座標）
(defun r   (id) (funcall (third (nd id))))              ; realize：遞迴組出子節點
(defun str (&rest xs) (apply #'concatenate 'string (mapcar (lambda (x) (or x "")) xs)))
(defun join (xs) (format nil "~{~a~^~%~}" xs))
(defun pk  (slot cands) (nth (or (cdr (assoc slot *env* :test #'equal)) 0) cands))  ; 確定性選擇
(defun sp  (who body) (str who "：「" body "」"))        ; 台詞槽的角色框

;;; ══ 護欄邏輯（內部節點可掛任意多條・示範兩條）═══════════════════════════════
(defparameter *配* '(("風景" "變了" "變樣") ("櫻花" "謝了" "落了")   ; 母題×變化動詞相配表
                     ("季節" "換了" "變了") ("大家" "都往前走了")))
(defparameter *canon* '("這條路" "河堤" "河邊" "車站" "這裡"))        ; 收束護欄：錨只能引 canon
(defun guard (m v)                                      ; 內部節點護欄①：變項×動詞須語意相配
  (unless (member v (cdr (assoc m *配* :test #'string=)) :test #'string=)
    (error "護欄：母題「~a」配不上變化動詞「~a」" m v)) t)

;;; ══ 河堤場景：從最粗（L0 整場八股）到最細（L5 原子事實）── 全用同一個 (node) ══════

;;; ── L0 場景（八股整場）：唯一最粗節點，引用五段 ───────────────────────────
(node 場景 0 (join (list (r "段A") (r "段B") (r "段C") (r "段D") (r "段E"))))

;;; ── L1 段落 A–E：內部節點，把旁白框（葉）＋台詞槽（子）依固定八股序組起 ────────
(node 段A 1 (join (list (r "旁1") (r "a1") (r "a2"))))
(node 段B 1 (join (list (r "旁2") (r "b1"))))
(node 段C 1 (join (list (r "c1") (r "c2"))))
(node 段D 1 (join (list (r "d1") (r "旁3") (r "d2"))))
(node 段E 1 (join (list (r "e1") (r "旁4"))))

;;; ── L2 旁白框（葉・固定）＋台詞槽（內部・角色框＋句式引用）─────────────────
;;;    ★旁白與台詞槽「同粒度、不同種」：一個是葉、一個是內部 → 證明「種 ≠ 粒度」。
(node 旁1 2 "斜陽把兩人的影子拉長，河水泛著細碎的金光。兩側櫻堤落下最後的櫻花雨。")
(node 旁2 2 "她停下腳步，看著飛舞的落櫻，眼神裡有一絲不易察覺的落寞。")
(node 旁3 2 "她微微一怔，轉過頭，落寞化作溫緩的笑意。")
(node 旁4 2 "她輕輕點頭，與我並肩踏上歸途，兩人的影子在水泥路上重疊在一起。最後的櫻花瓣落在她的髮梢。")
(node a1 2 (sp "秋穗" (r "T-a1")))
(node a2 2 (sp "主角" (r "T-a2")))
(node b1 2 (sp "秋穗" (r "T-b1")))
(node c1 2 (sp "主角" (r "T-c1")))
(node c2 2 (sp "秋穗" (r "T-c2")))
(node d1 2 (sp "主角" (r (pk "d1式" '("T-d1a" "T-d1b")))))  ; ★多句式＝搜索維度（風格/成本）
(node d2 2 (sp "秋穗" (r "T-d2")))
(node e1 2 (sp "主角" (r "T-e1")))

;;; ── L3 句式模板：內部節點，字面骨架＋填充物引用（＝八股的「格架」）───────────
(node T-a1 3 (str "結果，還是" (r "收束動詞") (r "收束地") "了呢。"))
(node T-a2 3 (str (r "順路句") "。" (r "景讚句") "。"))
(node T-b1 3 (str (r "外界觀察") "。有時候……" (r "怕的輕描") "。"))
(node T-c1 3 (str (r "怕字引") "什麼？"))
(node T-c2 3 (str "怕" (r "停滯句") "。也怕" (r "變樣句") "。"))
;; d1 兩式：帶「大量邏輯判斷」的內部節點──組合前先過護欄（變項×動詞相配）
(node T-d1a 3 (let ((m (r "母題")) (v (r "變化動詞")))
                (guard m v)
                (str (r "安定句") "。就算" m v "，" (r "不變的錨") (r "句尾"))))
(node T-d1b 3 (let ((m (r "母題")) (v (r "變化動詞")))
                (guard m v)
                (str m "會" v "，可是" (r "錨名") "不會。")))
(node T-d2 3 (str (r "語氣詞") "……" (r "認可") "。謝謝你。"))
(node T-e1 3 (str "走吧，" (r "催句") "。"))

;;; ── L4 填充物／原子事實（葉）：字面候選、母題導出、可空──都是「值＋邏輯」───────
(node 收束動詞 4 (pk "收束動詞" '("來到" "走到" "繞回")))
(node 收束地   4 (pk "收束地"   '("河堤" "河邊" "這裡")))
(node 順路句   4 (pk "順路句"   '("回車站剛好順路" "反正回車站順路嘛")))
(node 景讚句   4 (pk "景讚句"   '("今天的風真舒服" "河水都被曬成金的了")))
(node 外界觀察 4 (pk "外界觀察" '("新學期開始了，大家都在往前走" "花都謝了")))
(node 怕的輕描 4 (pk "怕的輕描" '("會有點害怕呢" "就覺得有點站不住腳")))
(node 怕字引   4 (pk "怕字引"   '("害怕" "在怕")))
(node 停滯句   4 (pk "停滯句"   '("自己停在原地" "自己跟不上")))
(node 母題     4 (pk "母題"     '("風景" "櫻花" "季節")))
(node 變化動詞 4 (pk "變化動詞" '("變了" "變樣" "謝了")))
(node 安定句   4 (pk "安定句"   '("沒關係的" "別怕" "我在啊")))
(node 句尾     4 (pk "句尾"     '("嗎？" "，不是嗎？" "。")))
(node 錨名     4 (pk "錨名"     '("我們一起走的這條路" "陪你的這個人")))
(node 語氣詞   4 (pk "語氣詞"   '("嗯" "……啊" "是喔")))
(node 認可     4 (pk "認可"     '("你說得對" "說得也是呢" "")))  ; ★可空槽＝葉的條件邏輯
(node 催句     4 (pk "催句"     '("電車快來了" "電車要進站了")))
(node 變樣句   4 (format nil "那些熟悉的~a，不知不覺就變了樣" (r "母題")))  ; ★母題導出＝葉帶邏輯
;; 「不變的錨」這個填充物其實還能再拆──它是內部節點，引用更深的 canon 原子事實（L5）
(node 不變的錨 4 (str "我們不是還能像現在這樣，一起走在" (r "canon地點") "上"))

;;; ── L5 canon 地點（原子事實・最細）：不變的錨再往下拆一層 → 證明軸「可任意深」 ────
(node canon地點 5 (let ((x (pk "canon地點" '("這條路" "河堤" "咖啡廳"))))  ; 內含護欄②：收束不出 canon
                    (unless (member x *canon* :test #'string=)
                      (error "收束護欄：錨引入非 canon 地點「~a」" x))
                    x))

;;; ══ 自帶 assert：①組出整場 ②確定性逐位元 ③粒度層級可查 ④護欄會擋 ════════════
(defun 整場 () (let ((*env* nil)) (r "場景")))

(let ((s1 (整場)) (s2 (整場)))
  ;; ① 能組出整場台詞（旁白框＋五段台詞，全由同一個 (node) macro 遞迴組出）
  (assert (search "斜陽把兩人的影子拉長" s1))
  (assert (search "秋穗：「結果，還是來到河堤了呢。」" s1))
  (assert (search "主角：「沒關係的。就算風景變了，我們不是還能像現在這樣，一起走在這條路上嗎？」" s1))
  (assert (search "秋穗：「嗯……你說得對。謝謝你。」" s1))
  (assert (search "最後的櫻花瓣落在她的髮梢" s1))
  ;; ② 確定性逐位元：同輸入兩次跑，長度＋逐字元＋checksum 全等
  (assert (string= s1 s2))
  (assert (= (length s1) (length s2)))
  (let ((c1 (reduce #'+ s1 :key #'char-code)) (c2 (reduce #'+ s2 :key #'char-code)))
    (assert (= c1 c2))
    (format t "確定性 checksum＝~a（長度 ~a）~%" c1 (length s1)))
  ;; ③ 粒度層級可查：同一種節點鋪在一條「連續的粒度軸」上（不是四分法的 4 個離散類）
  (assert (= 0 (lvl "場景"))) (assert (= 1 (lvl "段A"))) (assert (= 2 (lvl "a1")))
  (assert (= 2 (lvl "旁1")))              ; ★旁白（葉）與台詞槽（內部）同在 L2 → 種≠粒度
  (assert (= 3 (lvl "T-d1a"))) (assert (= 4 (lvl "收束動詞"))) (assert (= 5 (lvl "canon地點")))
  (let ((axis (sort (remove-duplicates
                     (loop for v being the hash-values of *ns* collect (second v))) #'<)))
    (assert (equal axis '(0 1 2 3 4 5)))  ; 六級連續軸，已 > 八股/段落/句式/填充物的 4 類
    (format t "粒度軸＝~a（~a 級，超過四分法的 4 類）~%" axis (length axis)))
  ;; ④ 護欄邏輯（內部節點）會擋不合法組合（母題風景×動詞謝了 不相配、錨引入非 canon）
  (assert (nth-value 1 (ignore-errors (let ((*env* '(("變化動詞" . 2)))) (r "T-d1a")))))
  (assert (nth-value 1 (ignore-errors (let ((*env* '(("canon地點" . 2)))) (r "不變的錨")))))
  ;; ⑤ 粒度軸「可調」：換風格座標，同一棵樹產出不同整場（確定性地）
  (let ((s3 (let ((*env* '(("d1式" . 1) ("母題" . 1) ("變化動詞" . 2) ("錨名" . 1)))) (r "場景"))))
    (assert (search "主角：「櫻花會謝了，可是陪你的這個人不會。」" s3))  ; 換句式＋換母題＝另一整場
    (assert (not (string= s1 s3))))

  (format t "~%── 整場河堤台詞（*env* 預設座標）──~%~a~%" s1)
  (format t "~%全部 assert 通過。節點總數＝~a，全部出自同一個 (node) macro。~%"
          (hash-table-count *ns*)))
