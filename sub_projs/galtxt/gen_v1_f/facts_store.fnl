;; facts_store.fnl — 編譯期寫入門①（add 入庫）＋ modify（canon 鎖）＋知識視圖＋dump。
;; 總則見 facts.fnl：確定性遍歷走 list 插入序；不變式架在門上。

(local U (require :facts_util))
(local S {})

;;; ------------------------------------------------ 編譯期寫入門①：add（入庫）

;; 檢查：ID 唯一、連結必有 refs、refs 必存在（佔位豁免）→ 接地不變式由歸納保證
(fn S.add [self f]
  (assert (and f.id (= (. self.by_id f.id) nil)) (.. "ID 缺失或重複：" (tostring f.id)))
  (assert (or (= f.kind "節點") (= f.kind "連結"))
          (.. "kind 必須是 節點/連結：" (tostring f.kind)))
  (when (= f.kind "連結")
    (assert (and (= (type f.refs) "table") (>= (length f.refs) 1))
            (.. "連結必須有 refs：" f.id))
    (each [_ r (ipairs f.refs)]
      (when (and (not (U.is_placeholder r)) (= (. self.by_id r) nil))
        (error (.. "接地失敗：refs 指向不存在的事實 " (tostring r) "（佔位請用 ? 前綴）") 0))))
  (when f.parent
    (assert (. self.by_id f.parent) (.. "parent 指向不存在的事實：" (tostring f.parent))))
  (set f.canon (or f.canon false))
  (table.insert self.list f)
  (tset self.by_id f.id f)
  f.id)

;;; ------------------------------------------------ modify（未呈現可改；canon 鎖）

(fn S.modify [self id value]
  (let [f (assert (. self.by_id id) (.. "未知事實：" (tostring id)))]
    (when f.canon
      (error (.. "canon 鎖：已呈現的事實不可改（只可再細化）：" id) 0))
    (set f.value value)))

;;; ------------------------------------------------ 知識視圖（柱二）

(fn S.set_knowledge [self who t]
  (tset self.knowledge who
        { :knows (or t.knows []) :hides (or t.hides []) :variants (or t.variants {}) }))

;; 視角＝overlay：全知恆等；角色＝variants 先替換（誤會）、knows∪hides 可見、其餘不可見
(fn S.view [self who]
  (if (= who "全知")
      { :who who :map (fn [id] id) }
      (let [k (assert (. self.knowledge who) (.. "沒有這個角色的知識視圖：" (tostring who)))]
        { :who who
          :map (fn [id]
                 (if (. k.variants id) (. k.variants id)          ; 誤會：映到錯誤變體
                     (or (U.has k.knows id) (U.has k.hides id)) id
                     (values nil "不可見"))) })))

;;; ------------------------------------------------ dump（測試用：全庫確定性快照）

(fn S.dump [self]
  (let [parts []]
    (each [_ f (ipairs self.list)]
      (table.insert parts (string.format "%s|%s|%s|%s"
                            f.id (tostring f.value) (tostring f.canon) (tostring f.finer))))
    (let [names []]
      (each [who (pairs self.knowledge)] (table.insert names who))
      (table.sort names)   ; 排序後再輸出：快照也要確定性
      (each [_ who (ipairs names)]
        (let [k (. self.knowledge who)]
          (table.insert parts (.. who "|" (table.concat k.knows ",")
                                  "|" (table.concat k.hides ","))))))
    (table.concat parts "\n")))

S
