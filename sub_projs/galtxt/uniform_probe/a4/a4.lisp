;;;; a4.lisp — 統一節點 kernel（galtxt / 場景二・霞川河堤）
;;;; 核心主張：八股／段落／句式／填充物／原子事實 都不是不同「種類」，只是不同「粒度」。
;;;;   一種節點 = (lambda (c) 字串)。粒度任意：字面、帶邏輯的填充物、句式、整場 全同型。
;;;;   句式＝「引用較細節點」的較粗節點；填充物＝「帶邏輯（讀 ctx）」的細節點——兩者無界線。
;;;; 跑：sbcl --script a4.lisp   （自帶 assert，全綠才印範例）

;;; ── ctx：節點唯一的輸入。母題/情緒/角色/嘴硬/前文(回) 全放這 ───────────────
(defun cx (&rest kv)                                   ; 造 ctx（keyword→值）
  (let ((h (make-hash-table))) (loop for (k v) on kv by #'cddr do (setf (gethash k h) v)) h))
(defun cp (c)                                          ; 複製 ctx（整場逐行換角色用）
  (let ((h (make-hash-table))) (maphash (lambda (k v) (setf (gethash k h) v)) c) h))
(defun 回 (&rest ss)                                    ; 造「前文已出字面」集合
  (let ((h (make-hash-table :test 'equal))) (dolist (s ss h) (setf (gethash s h) t))))
;;; 節點內邏輯就靠這幾個存取器（外加 pos / mo）——填充物的「判斷力」全來自它們
(defun m (c) (gethash :母題 c :風景))                    ; 母題
(defun e (c) (gethash :情緒 c :靜))                      ; 情緒
(defun r (c) (gethash :角色 c :主角))                    ; 說話角色
(defun 硬 (c) (gethash :嘴硬 c))                         ; 秋穗是否嘴硬
(defun seen (c s) (gethash s (gethash :回 c (make-hash-table :test 'equal))))  ; 前文是否出過 s
(defun pos (x l) (or (position x l) 0))                ; 情緒→索引 的小工具

;;; ── 母題表：可插拔隱喻軸。同一句式換母題→換字面，就是靠這張表 ─────────────
(defparameter *母題表*
 '((:櫻   :變項 "櫻花" :動 "謝了"   :變樣 "那些熟悉的花，一夜之間就謝了"     :景 "最後的櫻花雨落在泛金的水面")
   (:河水 :變項 "河水" :動 "流遠了" :變樣 "看慣的河景，哪天就流走了樣"       :景 "河水都被斜陽曬成金的了")
   (:影子 :變項 "影子" :動 "散了"   :變樣 "並肩的影子，說不定哪天就不並排了" :景 "斜陽把兩人的影子拉得好長")
   (:電車 :變項 "班次" :動 "改點了" :變樣 "連這班電車，哪天也悄悄改了點"     :景 "遠處傳來電車進站的聲音")
   (:風   :變項 "風向" :動 "轉了"   :變樣 "風一轉，熟悉的氣味就散了"         :景 "晚風捲起一地落櫻")
   (:季節 :變項 "季節" :動 "換了"   :變樣 "季節一換，這場雪就落盡了"         :景 "花期正一步步走到終點")
   (:風景 :變項 "風景" :動 "變樣了" :變樣 "那些熟悉的風景，不知不覺就變了樣" :景 "傍晚的河堤靜得只剩腳步聲")))
(defun mo (c k) (getf (cdr (assoc (m c) *母題表*)) k))

;;; ══ kernel（全部 macro）════════════════════════════════════════════════════
;;; 節點形（cn 編譯期折成讀 c 的字串表達式）：
;;;   "字面"            → 字面
;;;   (& a b …)         → 序列拼接        ＝句式（較粗節點）
;;;   (pk sel a b …)    → 依 sel(讀 c) 選一個子節點   ＝帶邏輯的填充物
;;;   (?? pred a [b])   → 條件；b 省略＝可空槽（false→""）
;;;   (@ 名)            → 引用具名節點     ＝遞迴／跨粒度連結
;;;   (f 任意lisp)      → 逃生艙：任意讀 c 的邏輯回字串（填充物可帶「大量」判斷）
;;; sel/pred/f 內可直接用 m e r 硬 seen pos mo——這就是填充物的「判斷力」來源。
(eval-when (:compile-toplevel :load-toplevel :execute)
  (defun cn (x)
    (cond ((stringp x) x)
          ((not (consp x)) x)
          (t (case (car x)
               (&  `(concatenate 'string ,@(mapcar #'cn (cdr x))))
               (pk (let ((g (gensym)))
                     `(let ((,g (list ,@(mapcar (lambda (y) `(lambda () ,(cn y))) (cddr x)))))
                        (funcall (nth (mod ,(cadr x) (length ,g)) ,g)))))
               (?? `(if ,(cadr x) ,(cn (caddr x)) ,(if (cdddr x) (cn (cadddr x)) "")))
               (@  `(run ,(string-downcase (string (cadr x))) c))
               (f  (cadr x))
               (t  (error "未知節點形 ~a" x)))))))
(defmacro nd (form) `(lambda (c) (declare (ignorable c)) ,(cn form)))          ; 節點＝閉包
(defvar *nodes* (make-hash-table :test 'equal))
(defmacro def! (name form) `(setf (gethash ,(string-downcase (string name)) *nodes*) (nd ,form)))
(defun run (name c) (funcall (or (gethash name *nodes*) (error "無節點 ~a" name)) c))

;;; ── 填充物（細節點）：每一個都「帶邏輯」——依母題/情緒/角色/嘴硬/前文 分歧 ──
(def! 安定    (pk (pos (e c) '(:怕 :靜 :甜 :微酸)) "別怕" "沒關係的" "我在啊" "我懂"))       ; 依情緒
(def! 收束動詞 (pk (pos (e c) '(:靜 :怕 :甜)) "來到" "走到" "繞回"))                          ; 依情緒
(def! 收束地  (pk (if (eq (m c) :河水) 1 0) "河堤" "河邊"))                                   ; 依母題
(def! 順路句  (pk (if (硬 c) 1 0) "回車站剛好順路" "反正回車站順路嘛"))                        ; 依嘴硬
(def! 景讚    (?? (seen c "金") "傍晚的風真舒服" (f (mo c :景))))                              ; 依前文（避重）
(def! 外界觀察 (& (f (mo c :變項)) "也在變，大家好像都準備好往前衝了"))                        ; 依母題（內含更細節點）
(def! 怕的輕描 (?? (硬 c) "我才不會不安呢" "會有點害怕呢"))                                     ; 依嘴硬（角色語氣）
(def! 怕字引  (pk (pos (r c) '(:主角 :秋穗)) "害怕" "在怕"))                                   ; 依角色
(def! 停滯句  (pk (if (eq (e c) :微酸) 1 0) "自己停在原地" "大家都往前，只有我留在原地"))       ; 依情緒
(def! 變樣句  (f (mo c :變樣)))                                                               ; 依母題
(def! 會變項  (f (mo c :變項)))                                                               ; 依母題
(def! 變化動詞 (f (mo c :動)))                                                                ; 依母題（與會變項相配）
(def! 不變的錨 (?? (eq (e c) :甜) (@ 約定) "我們還能一起走這條路"))                            ; 依情緒＋遞迴
(def! 約定    (& "說好，明年這時候，還來這條河堤看一次" (f (mo c :變項)) "，好不好？"))          ; 較粗填充物＝句式
(def! 句尾    (pk (pos (e c) '(:靜 :甜 :微酸 :怕)) "。" "……啊。" "，不是嗎？" "，好嗎？"))     ; 依情緒
(def! 語氣詞  (pk (pos (e c) '(:甜 :靜 :微酸 :釋懷)) "嗯" "……啊" "……" "嗯"))                ; 依情緒
(def! 認可尾  (?? (member (e c) '(:甜 :釋懷)) "你說得對。謝謝你。" "謝謝你。"))                 ; 可空側＝只道謝
(def! 催句    (pk (if (eq (m c) :電車) 1 0) "電車快來了" "電車要進站了"))                       ; 依母題

;;; ── 句式（較粗節點）：只是「引用填充物」的序列。與填充物同型、同 def!、同 run ──
(def! a1 (& "結果，還是" (@ 收束動詞) (@ 收束地) "了呢。"))
(def! a2 (& (@ 順路句) "。" (@ 景讚) "。"))
(def! b1 (& (@ 外界觀察) "。有時候……" (@ 怕的輕描) "。"))
(def! c1 (& (@ 怕字引) "什麼？"))
(def! c2 (& "怕" (@ 停滯句) "。也怕" (@ 變樣句) "。"))
(def! d1 (& (@ 安定) "。就算" (@ 會變項) (@ 變化動詞) "，" (@ 不變的錨) (@ 句尾)))
(def! d2 (& (@ 語氣詞) "……" (@ 認可尾)))
(def! e1 (& "走吧，" (@ 催句) "。"))

;;; ── 整場（最粗節點）：固定旁白框 ＋ 逐行換角色跑句式。也只是一個 seq ──────────
(defun 場 (base)
  (labels ((ln (nm who) (let ((c (cp base))) (setf (gethash :角色 c) who)
                          (format nil "~a：「~a」" (string who) (run nm c)))))
    (format nil "~{~a~^~%~}"
      (list "（斜陽把兩人的影子拉長，河水泛著細碎的金光。兩側櫻堤落下最後的櫻花雨。）"
            (ln "a1" :秋穗) (ln "a2" :主角)
            "（她停下腳步，看著飛舞的落櫻，眼神裡有一絲不易察覺的落寞。）"
            (ln "b1" :秋穗) (ln "c1" :主角) (ln "c2" :秋穗)
            (ln "d1" :主角) "（她微微一怔，轉過頭，落寞化作溫緩的笑意。）" (ln "d2" :秋穗)
            (ln "e1" :主角)
            "（她輕輕點頭，並肩踏上歸途，最後的櫻花瓣落在她的髮梢。）"))))

;;; ══ assert：kernel 的行為契約 ══════════════════════════════════════════════
;; ① 同一句式在不同母題下填出不同字面（c2 帶母題槽 變樣句）
(let ((櫻 (run "c2" (cx :母題 :櫻))) (河 (run "c2" (cx :母題 :河水))))
  (assert (string/= 櫻 河)) (assert (search "花" 櫻)) (assert (search "河" 河)))
;; ② 同一句式在不同情緒下填出不同字面（d1 安定/句尾/錨 全依情緒；甜→錨遞迴成「約定」）
(let ((怕 (run "d1" (cx :情緒 :怕 :母題 :櫻))) (甜 (run "d1" (cx :情緒 :甜 :母題 :櫻))))
  (assert (string/= 怕 甜))
  (assert (search "說好" 甜)) (assert (not (search "說好" 怕))))          ; 遞迴：甜才展開子句式
;; ③ 帶邏輯的填充物確實分歧——依角色（怕字引）、依嘴硬（怕的輕描）
(assert (string/= (run "怕字引" (cx :角色 :秋穗)) (run "怕字引" (cx :角色 :主角))))
(assert (string/= (run "怕的輕描" (cx :嘴硬 t)) (run "怕的輕描" (cx :嘴硬 nil))))
;; ④ 可空槽正確——認可尾：靜(空側，只道謝) vs 甜(有認可句)
(let ((靜 (run "d2" (cx :情緒 :靜))) (甜 (run "d2" (cx :情緒 :甜))))
  (assert (not (search "你說得對" 靜))) (assert (search "你說得對" 甜))
  (assert (search "謝謝你" 靜)))                                          ; 空槽被跳過但句子仍完整
;; ⑤ 依前文（回）選字——景讚：前文出現過「金」就避開金景
(assert (search "金" (run "景讚" (cx :母題 :河水))))
(assert (not (search "金" (run "景讚" (cx :母題 :河水 :回 (回 "金"))))))
;; ⑥ 遞迴節點確為「填充物內含更細句式」——不變的錨 甜→整句約定、靜→死字串
(assert (search "說好" (run "不變的錨" (cx :情緒 :甜 :母題 :季節))))
(assert (not (search "說好" (run "不變的錨" (cx :情緒 :靜)))))

;;; ══ 全綠→印兩種座標的整場（同一 kernel、不同 ctx，確定性產出）═══════════════
(format t "~&assert 全數通過。~%~%=== 座標 A〔母題:櫻・情緒:甜〕===~%~a~%~%" (場 (cx :母題 :櫻 :情緒 :甜)))
(format t "=== 座標 B〔母題:河水・情緒:微酸・秋穗嘴硬〕===~%~a~%" (場 (cx :母題 :河水 :情緒 :微酸 :嘴硬 t)))
