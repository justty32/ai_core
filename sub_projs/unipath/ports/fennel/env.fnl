;; env.fnl —— 執行態環境的核心：活物件圖 + 路徑導航（對照 unipath_env.py）。
;; 映射：list→數字子路徑（0-based）、dict→字串鍵子路徑；任何節點皆為目錄，
;; 含 Plan 9 慣例葉子 data/ctl/status。節點以 kind 標記型別。

(local LEAVES {:data true :ctl true :status true})

(fn scalar [pyt val] {:kind :scalar :py pyt :val val})

(fn build-world []
  ;; 同一棵示範樹：/0 counter(int)、/1 list、/2 dict
  {:kind :list
   :items
   [(scalar :int 0)
    {:kind :list :items [(scalar :str "alpha") (scalar :str "beta") (scalar :str "gamma")]}
    {:kind :dict :order ["name" "hp"]
     :map {:name (scalar :str "world") :hp (scalar :int 100)}}]})

(fn container? [node]
  (and node (or (= node.kind :list) (= node.kind :dict))))

(fn split [path]
  (let [out []]
    (each [seg (string.gmatch path "[^/]+")] (table.insert out seg))
    out))

(fn children [node]
  ;; list → "0".."n-1"；dict → 依序的鍵
  (if (= node.kind :list)
      (let [out []]
        (for [i 1 (length node.items)] (table.insert out (tostring (- i 1))))
        out)
      (= node.kind :dict) node.order
      []))

(fn child-key [node seg]
  (if (= node.kind :list)
      (let [i (tonumber seg)]
        (if (and i (= i (math.floor i)) (>= i 0) (< i (length node.items)))
            i (error "ENOENT")))
      (= node.kind :dict)
      (do (var found nil)
          (each [_ k (ipairs node.order)] (when (= k seg) (set found k)))
          (or found (error "ENOENT")))
      (error "ENOENT")))

(fn child-get [node k]
  (if (= node.kind :list) (. node.items (+ k 1)) (. node.map k)))

(fn child-set [node k v]
  (if (= node.kind :list) (tset node.items (+ k 1) v) (tset node.map k v)))

(fn resolve [world path]
  ;; → (parent key value leaf)；leaf 為 data/ctl/status 或 nil
  (let [parts (split path)]
    (var leaf nil)
    (when (and (> (length parts) 0) (. LEAVES (. parts (length parts))))
      (set leaf (table.remove parts)))
    (var parent nil) (var key nil) (var value world)
    (each [_ seg (ipairs parts)]
      (when (not (container? value)) (error "ENOENT"))
      (let [k (child-key value seg)]
        (set parent value) (set key k) (set value (child-get value k))))
    (values parent key value leaf)))

{: LEAVES : build-world : container? : children
 : child-set : resolve}
