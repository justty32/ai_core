;;;; realizer.lisp — 八股 realizer（Common Lisp）。模板即 list、macro 編譯期折 fill。
;;;; G(母題,風格座標)→整場河堤台詞。零 LLM/零 RNG、確定性。跑：sbcl --script realizer.lisp

;;; ── 同像性核心：(tmpl id 字面|槽…) → (list "ID" (槽…) fill)。字串=字面、符號=槽；
;;;    fill 編譯期折成 (concatenate 'string …)，執行期不解析字串——macro 之所在。
(defmacro tmpl (id &rest parts)
  (let* ((s (gensym)) (ks '())
         (fs (loop for p in parts collect
                   (if (stringp p) p (let ((n (string p))) (push n ks) `(gethash ,n ,s ""))))))
    `(list ,(string id) ',(nreverse ks) (lambda (,s) (concatenate 'string ,@fs)))))
(defun al (k a) (cdr (assoc k a :test #'string=)))   ; alist 查（字串鍵）

;;; ── 八股格架：固定 items 序（旁白框不變，只有槽可換台詞）──────────────
(defparameter *scene*
  '((:旁 "（斜陽把兩人的影子拉長，河水泛著細碎的金光。兩側櫻堤落下最後的櫻花雨。）")
    (:s "秋穗" "a1") (:s "主角" "a2")
    (:旁 "（她停下腳步，看著飛舞的落櫻，眼神裡有一絲不易察覺的落寞。）") (:s "秋穗" "b1")
    (:s "主角" "c1") (:s "秋穗" "c2")
    (:s "主角" "d1") (:旁 "（她微微一怔，轉過頭，落寞化作溫緩的笑意。）") (:s "秋穗" "d2")
    (:s "主角" "e1")
    (:旁 "（她輕輕點頭，與我並肩踏上歸途，兩人的影子在水泥路上重疊在一起。最後的櫻花瓣落在她的髮梢。）")))

;;; ── 每槽多句子模板（d1 三結構）＋填充物＋兩條相配資料 ──────────────────
(defparameter *tmpl*
  (list (cons "a1" (list (tmpl a1.1 "結果，還是" 收束動詞 收束地 "了呢。")
                         (tmpl a1.2 "你有沒有發現，我們" 頻率 收束動詞 收束地 "。")))
        (cons "a2" (list (tmpl a2.1 順路句 "。" 景讚句 "。")))
        (cons "b1" (list (tmpl b1.1 外界觀察 "。有時候……" 怕的輕描 "。")))
        (cons "c1" (list (tmpl c1.1 怕字引 "什麼？")))
        (cons "c2" (list (tmpl c2.1 "怕" 停滯句 "。也怕" 變樣句 "。")))
        (cons "d1" (list (tmpl d1.1 安定句 "。就算" 會變項 變化動詞 "，" 不變的錨 句尾)
                         (tmpl d1.2 會變項 "會" 變化動詞簡 "，可是" 錨名 "不會。")
                         (tmpl d1.3 安定句 "。有我" 陪伴行動 "，" 會變項 變化動詞 "也沒關係。")))
        (cons "d2" (list (tmpl d2.1 語氣詞 "……" 認可 "。謝謝你。")))
        (cons "e1" (list (tmpl e1.1 "走吧，" 催句 "。")))))
;; *fill*：槽→候選。值是 list（措辭候選，取序）或 fn（吃母題、回字面）——一種抽象兩型
(defparameter *fill*
  (list (cons "會變項" (lambda (m) m))
        (cons "變樣句" (lambda (m) (format nil "那些熟悉的~a，不知不覺就變了樣" m)))
        (cons "外界觀察" (lambda (m) (format nil "~a也在變，大家卻都往前走了" m)))
        '("安定句" "沒關係的" "別怕" "我在啊" "會沒事的")
        '("收束動詞" "來到" "走到" "繞回") '("收束地" "河堤" "河邊" "這裡")
        '("頻率" "總是" "老是" "每次")
        '("順路句" "回車站剛好順路" "反正回車站順路嘛") '("景讚句" "今天的風真舒服" "河水都被曬成金的了")
        '("怕的輕描" "會有點害怕呢" "就覺得有點站不住腳") '("怕字引" "害怕" "在怕")
        '("停滯句" "自己停在原地" "自己跟不上")
        '("變化動詞" "變了" "謝了" "換了" "都往前走了") '("變化動詞簡" "變" "謝" "換")
        '("不變的錨" "我們還能一起走這條路" "我還會在這條路上等你" "我們還能一起看下一場櫻花雪")
        '("錨名" "我們一起走的這條路" "陪你的這個人" "我們並肩的樣子")
        '("陪伴行動" "陪你走這條路" "在你旁邊") '("句尾" "，不是嗎？" "。" "……啊。")
        '("語氣詞" "嗯" "……啊" "是喔") '("認可" "你說得對" "說得也是呢" "") '("催句" "電車快來了" "電車要進站了")))
(defparameter *變項動詞*
  '(("櫻花" "謝了" "落了" "變了") ("風景" "變了" "變了樣") ("季節" "換了" "變了")
    ("小鎮" "變了" "變了樣") ("大家" "都往前走了") ("學校" "變了樣" "變了")))
(defparameter *非canon* '("咖啡廳" "陽溜亭" "天台" "教室" "禮堂"))

;;; ── 引擎：填槽→護欄→組場 ──────────────────────────────────────────
(defun fill-slot (name m &optional (i 1))          ; 母題函數→吃母題；否則取第 i 候選
  (let ((v (al name *fill*))) (if (functionp v) (funcall v m) (or (nth (1- i) v) (first v)))))

(defun ck (st m)                                   ; 護欄①變項×動詞相配 ②收束護欄 → (values ok why)
  (let ((v (gethash "變化動詞" st)) (a (or (gethash "不變的錨" st) (gethash "陪伴行動" st) (gethash "錨名" st))))
    (cond ((and v (not (member v (al m *變項動詞*) :test #'string=)))
           (values nil (format nil "變項×動詞不相配：「~a」配不上「~a」" m v)))
          ((and a (some (lambda (w) (search w a)) *非canon*))
           (values nil (format nil "收束護欄：錨引入非 canon——~a" a)))
          (t (values t nil)))))

(defun line (tm m &optional fills)                 ; 填模板所有槽→過護欄→折字面 → (values 台詞|nil id|why)
  (let ((st (make-hash-table :test 'equal)))
    (dolist (n (second tm)) (setf (gethash n st) (fill-slot n m (or (al n fills) 1))))
    (multiple-value-bind (ok why) (ck st m)
      (if ok (values (funcall (third tm) st) (first tm)) (values nil why)))))

(defun say (it m coord)                            ; 一個 :s item → 「台詞」（依 coord 選模板）
  (let ((tm (nth (1- (or (al (third it) coord) 1)) (al (third it) *tmpl*))))
    (multiple-value-bind (txt why) (line tm m)
      (or (and txt (format nil "~a：「~a」" (second it) txt)) (error "生成失敗＠~a：~a" (third it) why)))))

(defun scene-text (m &optional coord)              ; 走 scene：旁白直落、台詞經 say
  (format nil "~{~a~^~%~}"
          (loop for it in *scene* collect (if (eq (car it) :旁) (second it) (say it m coord)))))

(defun enum-d1 (m)                                 ; 段 D 三模板×合法動詞 → 多種台詞（示範多模板）
  (loop for tm in (al "d1" *tmpl*) append
        (loop for i from 1 to 4
              for r = (multiple-value-list (line tm m `(("變化動詞" . ,i) ("變化動詞簡" . ,i))))
              when (first r) collect (list (second r) (nth (1- i) (al "變化動詞" *fill*)) (first r)))))

;; demo 移至 main.lisp（load 定序：realizer→director→demo）。本檔＝realizer 核心。
