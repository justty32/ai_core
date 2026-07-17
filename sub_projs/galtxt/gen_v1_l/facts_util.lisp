;;;; facts_util.lisp — facts 子模組共用小工具（無狀態純函式）。

(defun has-p (lst v)
  "v 是否在 lst 裡（equal 比較，字串 ID 用）。"
  (and (member v lst :test #'equal) t))

(defun remove-val (lst v)
  "移除首個 v，回傳新 list（函數式版；呼叫端 setf 回槽位——對應 Lua 的就地 table.remove）。"
  (remove v lst :test #'equal :count 1))

;; 佔位連結：id「?」開頭＝連到還不存在的事實（懸空合法，柱四）
(defun placeholder-p (id)
  (and (stringp id) (plusp (length id)) (char= (char id 0) #\?)))

;; 排斥邊檢查：有無「排斥」邊連著 id、而另一端已 canon？（柱四：兩端不得同真）
(defun exclusion-block (db id)
  (loop for f across (db-facts db)
        when (and (equal (fget f :kind) "連結")
                  (equal (fget f :edge-type) "排斥")
                  (has-p (fget f :refs) id))
          do (dolist (other (fget f :refs))
               (let ((of (gethash other (db-by-id db))))
                 (when (and (not (equal other id)) of (fget of :canon))
                   (return-from exclusion-block other)))))
  nil)

;; 測試用：預期會噴錯的形式（對應 Lua 的 pcall→assert not ok）
(defmacro expect-error (&body body)
  `(handler-case (progn ,@body nil) (error () t)))
