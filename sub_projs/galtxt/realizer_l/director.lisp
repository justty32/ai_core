;;;; director.lisp — realizer 的 director：搜風格座標（模板×填充）、成本模型挑最優。
;;;; 前置 (load "realizer.lisp")。G 升格：direct(母題,intent[,history])→最優整場；
;;;; direct-n→N 種遞減優選台詞流（history 逼下輪換措辭，呼應最初手寫的「20 種」）。
;;;; 確定性：固定枚舉序、嚴格 < 才更新最優（先枚舉者贏平手）；歷史是輸入。

(defun list-slots (tm)                    ; tm 的 list 型槽（母題 fn 槽無索引、不枚舉）
  (remove-if (lambda (n) (functionp (al n *fill*))) (second tm)))

(defun enum-fills (tm)                    ; list 型槽的填充索引笛卡爾積 → fills-alist 清單
  (labels ((rec (ss) (if (null ss) (list '())
                         (loop for rest in (rec (cdr ss)) append
                               (loop for i from 1 to (length (al (car ss) *fill*))
                                     collect (acons (car ss) i rest))))))
    (rec (list-slots tm))))

(defun icost (intent txt)                 ; 風格意圖成本：甜＝暖標記、靜＝簡短平白
  (case intent
    (:甜 (+ (if (search "啊" txt) 0 4) (if (or (search "一起" txt) (search "陪" txt)) 0 4)))
    (:靜 (+ (floor (length txt) 4) (if (search "，不是嗎？" txt) 4 0)))
    (t 0)))

(defun best-line (slot m intent history)  ; 該槽窮舉模板×填充→過護欄→取最小成本（含 prior/intent/history）
  (let (bt bid (bc most-positive-fixnum))
    (loop for tm in (al slot *tmpl*) for ti from 0 do
          (dolist (f (enum-fills tm))
            (multiple-value-bind (txt id) (line tm m f)
              (when txt
                (let ((c (+ ti (reduce #'+ f :key (lambda (kv) (1- (cdr kv))) :initial-value 0)
                            (icost intent txt) (if (gethash txt history) 100 0))))
                  (when (< c bc) (setf bc c bt txt bid id)))))))
    (values bt bid)))

(defun direct (m intent &optional (history (make-hash-table :test 'equal)))
  (let (trace txts)                       ; 走 scene，每 :s 槽取 best-line；回 (values 整場 trace 選用句)
    (values (format nil "~{~a~^~%~}"
                    (loop for it in *scene* collect
                          (if (eq (car it) :旁) (second it)
                              (multiple-value-bind (txt id) (best-line (third it) m intent history)
                                (push id trace) (push txt txts)
                                (format nil "~a：「~a」" (second it) txt)))))
            (nreverse trace) (nreverse txts))))

(defun direct-n (m intent n)              ; N 種：每輪把用過的句加 history，逼下輪換措辭
  (let ((h (make-hash-table :test 'equal)) out)
    (dotimes (k n)
      (multiple-value-bind (s tr txts) (direct m intent h) (declare (ignore tr))
        (push s out) (dolist (tx txts) (setf (gethash tx h) t))))
    (nreverse out)))
