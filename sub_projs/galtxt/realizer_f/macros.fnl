;; macros.fnl — realizer 的同像性核心：句子模板本身就是一個 list（資料＝程式）。
;; 模板寫成 vector：字串＝字面、符號＝槽。macro 直接走訪這個 list、編譯期折成填充函式，
;; 不做任何字串解析——這才是 Lisp macro 的用法（homoiconic：模板即 AST）。
;;
;;   (tmpl :d1 ["就算" 會變項 變化動詞 "，" 不變的錨 "。"])
;;     展開成 {:id "d1"
;;             :slots ["會變項" "變化動詞" "不變的錨"]
;;             :fill (fn [st] (.. "就算" (. st "會變項") (. st "變化動詞") "，" (. st "不變的錨") "。"))}

;; (tmpl id 模板vector) → 模板記錄（含編譯期折好的 fill）
(fn tmpl [id parts]
  (let [st (gensym :st)            ; 衛生：param 與 (. st …) 共用同一 gensym
        cat (list (sym :..))       ; 累積 (.. …) 的參數
        slots []]
    (each [_ node (ipairs parts)]
      (if (= (type node) :string)
          (table.insert cat node)                    ; 字面
          (let [nm (tostring node)]                  ; 符號＝槽名
            (table.insert cat (list (sym :.) st nm))
            (table.insert slots nm))))
    (when (= (length cat) 1) (table.insert cat ""))  ; 全空保底
    `{:id ,id :slots ,slots :fill (fn [,st] ,cat)}))

{: tmpl}
