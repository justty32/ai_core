;; facts.fnl — 事實庫聚合入口：帶型別邊的分層 LOD 網（設計見 ../gen_v1/00_設計.md）。
;; gen_v1（Lua）的 Fennel 移植，行為逐條對齊。玩具版鐵則：純函式、無 RNG、無 IO、無時鐘；
;; 一切遍歷走 list 插入序（確定性）。讀寫不對稱：讀處處可讀；寫只有兩道門——
;;   編譯期門：add（入庫，facts_store）＋ refine（細化，facts_lod；解不出→掛起工單）
;;   執行期門：act 經 speculate 交易 commit（facts_tx；此時才鎖 canon、動知識視圖）
;; 不變式全架在門上：接地、父約束、段鏈首尾、排斥邊、canon 鎖、「說不出不知道的事」。
;; 識別子英文、值與 ID 可中文；單檔 ≤120 行、按職責拆子模組。

(local M {})
(tset M :__index M)

;; 子模組分工：store（門①＋視圖＋dump）／lod（門②）／query（讀取五動詞）／tx（執行期門）
(each [_ mod (ipairs [:facts_store :facts_lod :facts_query :facts_tx])]
  (each [name f (pairs (require mod))]
    (assert (= (. M name) nil) (.. "子模組方法名衝突：" name))
    (tset M name f)))

(fn M.new []
  (setmetatable
    { :list []        ; 事實陣列：插入序＝唯一合法遍歷序（確定性；禁 hash 序）
      :by_id {}
      :knowledge {}   ; [角色] = { knows=[id…] hides=[id…] variants={[真相id]=變體id} }
      :pending [] }   ; 掛起的編譯期工單（refine 解不出時排隊，等 LLM 炸候選→人審）
    M))

M
