;;;; facts.lisp — 事實庫聚合入口：帶型別邊的分層 LOD 網（設計見 ../gen_v1/00_設計.md）。
;;;; 玩具版鐵則同 Lua 版：純 CL、無 RNG、無 IO、無時鐘；一切遍歷走插入序 vector/list（確定性）。
;;;; 讀寫不對稱：讀處處可讀；寫只有兩道門——
;;;;   編譯期門：db-add（入庫，facts_store）＋ db-refine（細化，facts_lod；解不出→掛起工單）
;;;;   執行期門：act 經 speculate 交易 commit（facts_tx；此時才鎖 canon、動知識視圖）
;;;; 不變式全架在門上：接地、父約束、段鏈首尾、排斥邊、canon 鎖、「說不出不知道的事」。
;;;; 識別子英文、值與 ID 可中文（同 Lua 版慣例）；單檔 ≤120 行、按職責拆子模組。
;;;; 組織：純 load 腳本風格（無 package／ASDF），load 順序見 main.lisp。

;; 事實＝每筆一個 hash-table（鍵為 keyword）——對應 Lua 的自由欄位 table。
;; 只做鍵的點查，絕不遍歷其鍵（CL hash-table 遍歷序不確定——確定性鐵則）。
(defun fact (&rest kvs)
  "從 keyword plist 建一筆事實。"
  (let ((f (make-hash-table :test #'eq)))
    (loop for (k v) on kvs by #'cddr do (setf (gethash k f) v))
    f))

(defun fget (f key)
  "讀事實欄位；缺欄回 nil（對應 Lua 的 f.key == nil）。"
  (gethash key f))

(defun fset (f key v)
  (setf (gethash key f) v))

;; 角色知識視圖（柱二）：knows／hides 為 id list；variants＝誤會（真相id→變體id 的 alist）
(defstruct kn (knows nil) (hides nil) (variants nil))

;; 事實庫本體（對應 Lua 的 M.new()）
(defstruct (db (:constructor make-db ()))
  (facts (make-array 0 :adjustable t :fill-pointer 0))    ; 事實向量：插入序＝唯一合法遍歷序
  (by-id (make-hash-table :test #'equal))                 ; id→事實：只點查、不遍歷
  (knowledge nil)                                         ; alist（who . kn），插入序
  (pending (make-array 0 :adjustable t :fill-pointer 0))) ; 掛起的編譯期工單（等 LLM 炸候選→人審）

(defun get-knowledge (db who)
  "取角色知識視圖；沒有回 nil（旁白等無視圖者）。"
  (cdr (assoc who (db-knowledge db) :test #'equal)))
