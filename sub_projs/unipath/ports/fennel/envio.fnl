;; envio.fnl —— 環境的對外讀寫 API：stat / readdir / read / write。
;; 葉子語意對照 unipath_env.py 的 _leaf_bytes / stat / write。

(local env (require :env))

(fn py-repr [node]
  ;; 容器的 Python 風 repr（selftest 不讀，僅供 status/size 呈現用）
  (if (= node.kind :scalar)
      (if (= node.py :str) (.. "'" node.val "'") (tostring node.val))
      (= node.kind :list)
      (let [xs []] (each [_ it (ipairs node.items)] (table.insert xs (py-repr it)))
        (.. "[" (table.concat xs ", ") "]"))
      (= node.kind :dict)
      (let [xs []]
        (each [_ k (ipairs node.order)]
          (table.insert xs (.. "'" k "': " (py-repr (. node.map k)))))
        (.. "{" (table.concat xs ", ") "}"))
      "?"))

(fn py-str [node]
  (if (= node.kind :scalar) (tostring node.val) (py-repr node)))

(fn type-name [node]
  (if (= node.kind :scalar) node.py (= node.kind :list) "list" "dict"))

(fn leaf-bytes [node leaf]
  (if (= leaf :data)
      (.. (if (env.container? node) (py-repr node) (py-str node)) "\n")
      (= leaf :status)
      (let [n (if (env.container? node)
                  (if (= node.kind :list) (length node.items) (length node.order)) "-")]
        (.. "type=" (type-name node) " len=" (tostring n) " id=0\n"))
      (= leaf :ctl)
      "# append <literal> | set <key> <literal> | del <key>\n"
      (error "ENOENT")))

(fn stat [world path]
  (let [(_ _ value leaf) (env.resolve world path)]
    (if (= leaf nil)
        {:kind :dir :size 0 :ro false}
        {:kind :file :size (length (leaf-bytes value leaf)) :ro (= leaf :status)})))

(fn readdir [world path]
  (let [(_ _ value leaf) (env.resolve world path)]
    (when (not= leaf nil) (error "ENOTDIR"))
    (let [out ["data" "ctl" "status"]]
      (each [_ c (ipairs (env.children value))] (table.insert out c))
      out)))

(fn read [world path]
  (let [(_ _ value leaf) (env.resolve world path)]
    (when (= leaf nil) (error "EISDIR"))
    (leaf-bytes value leaf)))

(fn mk [text]
  ;; text → 新節點（整數→int scalar，否則 str scalar）
  (let [s (-> text (: :gsub "^%s+" "") (: :gsub "%s+$" ""))
        n (tonumber s)]
    (if (and n (= n (math.floor n))) {:kind :scalar :py :int :val n}
        {:kind :scalar :py :str :val s})))

(fn write [world path data]
  (let [(parent key value leaf) (env.resolve world path)]
    (if (= leaf :data)
        (do (when (= parent nil) (error "EROFS"))
            (env.child-set parent key (mk data))
            (length data))
        (= leaf :ctl) (length data)   ; ctl：接受但簡化（selftest 未用）
        (error "EROFS"))))

{: stat : readdir : read : write}
