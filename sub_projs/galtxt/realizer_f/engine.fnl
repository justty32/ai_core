;; engine.fnl — realizer 引擎：填槽→過護欄→組台詞。確定性：無 RNG，固定序、缺省取首。
;; G(母題, 風格座標) → 整場台詞。座標 coord：slot名→{:t 模板序 :f {槽名 候選序}}。

(local {: scene} (require :scene))
(local {: templates} (require :templates))
(local {: 變項動詞 : 母題填 : 自由填 : 非canon地} (require :fillers))

(fn member? [x xs] (var f false) (each [_ v (ipairs xs)] (when (= v x) (set f true))) f)

(fn 有非canon? [s]
  (var f false)
  (each [_ w (ipairs 非canon地)] (when (string.find s w 1 true) (set f true)))
  f)

;; 填一個槽：母題相關→由母題推；否則自由填取指定候選（缺省第一個）
(fn fill-slot [name 母題 idx]
  (let [mf (. 母題填 name)]
    (if mf (mf 母題)
        (let [opts (or (. 自由填 name) [""])]
          (or (. opts (or idx 1)) (. opts 1))))))

;; 護欄：① 變項×動詞相配 ② 收束護欄（錨不得引入新場景）
(fn check [st 母題]
  (let [動 (. st "變化動詞")
        錨 (or (. st "不變的錨") (. st "陪伴行動") (. st "錨名"))]
    (if (and 動 (not (member? 動 (or (. 變項動詞 母題) []))))
        (values false (.. "變項×動詞不相配：「" 母題 "」配不上「" 動 "」"))
        (and 錨 (有非canon? 錨))
        (values false (.. "收束護欄：錨引入了非 canon 場景——" 錨))
        (values true))))

;; nil 安全：coord 是 {slot {:t … :f …}}；k 為變數鍵，故不用 ?.（?. 對變數鍵不可靠）
(fn cslot [coord slot] (and coord (. coord slot)))

;; 生一句：填該模板所有槽→過護欄→折字面
(fn line [tmpl 母題 fills]
  (let [st {}]
    (each [_ nm (ipairs tmpl.slots)]
      (tset st nm (fill-slot nm 母題 (and fills (. fills nm)))))
    (let [(ok why) (check st 母題)]
      (if ok {:ok true :text (tmpl.fill st) :id tmpl.id}
          {:ok false :why why :id tmpl.id}))))

(fn pick-tmpl [slot coord]
  (let [c (cslot coord slot)
        idx (or (and c (. c :t)) 1)]
    (. (. templates slot) idx)))

;; 組整場：走 scene，旁白直落、台詞經 line。任一槽違規即報錯（不靜默）。
(fn scene-text [母題 coord]
  (let [buf []]
    (each [_ 段 (ipairs scene)]
      (each [_ it (ipairs 段.items)]
        (if it.旁 (table.insert buf it.旁)
            (let [c (cslot coord it.slot)
                  r (line (pick-tmpl it.slot coord) 母題 (and c (. c :f)))]
              (if r.ok (table.insert buf (.. it.say "：「" r.text "」"))
                  (error (.. "生成失敗＠" it.slot "（" r.id "）：" r.why) 0))))))
    (table.concat buf "\n")))

;; 列舉：對某槽窮舉其所有模板×合法變化動詞，回多種台詞（確定性序，示範多模板生成）
(fn enum-line [slot 母題]
  (let [out []]
    (each [ti tmpl (ipairs (. templates slot))]
      (each [fi 動 (ipairs (. 自由填 "變化動詞"))]
        (let [r (line tmpl 母題 {"變化動詞" fi "變化動詞簡" fi})]
          (when r.ok (table.insert out {:t ti :動 動 :text r.text :id r.id})))))
    out))

{: line : scene-text : enum-line : check}
