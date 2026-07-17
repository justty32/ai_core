;; facts_tx.fnl — 執行期寫入門：speculate 交易（begin→apply→check→commit/rollback）。
;; DFS 分支＝begin→apply→check→rollback；整條 act 序列定案才 commit。
;; act = { speaker=…, act=…, refs=[id…], effects={ reveal=[id…], to=角色 } }

(local U (require :facts_util))
(local T {})

;; 呈現＝鎖 canon。投影含粗事實與構件：連結呈現則其 refs 也算呈現、細值呈現則父鏈也算。
(fn present [db id]
  (when (not (U.is_placeholder id))
    (let [f (assert (. db.by_id id))]
      (when (not f.canon)
        (let [blocker (U.exclusion_block db id)]
          (when blocker
            (error (string.format "排斥邊擋下：%s 與已 canon 的 %s 不可同真" id blocker) 0)))
        (set f.canon true)
        (when f.parent (present db f.parent))
        (when (= f.kind "連結")
          (each [_ r (ipairs f.refs)] (present db r)))))))

;; 單一 act 的不變式檢查；回傳 nil（過）或錯誤訊息（擋）
(fn check-act [db act]
  (var err nil)
  (let [k (. db.knowledge act.speaker)]   ; 旁白等無視圖者不受「知道才能說」約束
    (each [_ r (ipairs (or act.refs []))]
      (when (= err nil)
        (if (U.is_placeholder r)
            (set err (.. "引用佔位事實（需先細化）：" r))
            (= (. db.by_id r) nil)
            (set err (.. "引用不存在的事實：" r))
            (and k (not (or (U.has k.knows r) (U.has k.hides r))))
            (set err (string.format "矛盾防線：%s 說出了自己不知道的事實 %s" act.speaker r))
            (let [blocker (U.exclusion_block db r)]
              (when (and blocker (not= blocker r))
                (set err (string.format "排斥邊：%s 與已 canon 的 %s 不可同真" r blocker)))))))
    (each [_ r (ipairs (or (?. act :effects :reveal) []))]
      (when (and (= err nil) (not (and k (U.has k.hides r))))
        (set err (string.format "洩底無效：%s 沒有藏著 %s" act.speaker r)))))
  err)

(fn T.begin [self]
  (let [db self
        tx { :acts [] }]
    (fn tx.apply [act] (table.insert tx.acts act))
    (fn tx.check []
      (var err nil)
      (each [_ act (ipairs tx.acts)]
        (when (= err nil) (set err (check-act db act))))
      (if err (values false err) true))
    (fn tx.commit []
      (let [(ok why) (tx.check)]
        (when (not ok) (error (.. "寫入門拒絕：" why) 0)))
      (each [_ act (ipairs tx.acts)]
        (each [_ r (ipairs (or act.refs []))] (present db r))
        (let [eff (or act.effects {})
              k (. db.knowledge act.speaker)]
          (each [_ r (ipairs (or eff.reveal []))]
            ;; 洩底＝知識轉移：hides→已表達（自己 knows）、聽者 knows 增加、誤會變體被真相替換
            (U.remove_val k.hides r)
            (when (not (U.has k.knows r)) (table.insert k.knows r))
            (let [lk (and eff.to (. db.knowledge eff.to))]
              (when lk
                (when (not (U.has lk.knows r)) (table.insert lk.knows r))
                (tset lk.variants r nil)))
            (present db r))))
      (tset tx :acts []))
    (fn tx.rollback [] (tset tx :acts []))
    tx))

T
