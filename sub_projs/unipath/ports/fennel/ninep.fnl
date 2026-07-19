;; ninep.fnl —— 9P2000 server 的共用原語：型別常數、qid、stat 編碼、錯誤。
;; 逐位元組相容 unipath_9p.py 的 _qid / _stat_bytes / _err。分派邏輯見 dispatch.fnl。

(local c (require :codec))
(local io* (require :envio))

;; 訊息型別（偶數 T、奇數 R = T+1）
(local T {:version 100 :attach 104 :flush 108 :walk 110 :open 112
          :create 114 :read 116 :write 118 :clunk 120 :remove 122
          :stat 124 :wstat 126})
(local R {:version 101 :attach 105 :error 107 :flush 109 :walk 111
          :open 113 :read 117 :write 119 :clunk 121 :stat 125})
(local ERRNO {:ENOENT "no such file" :ENOTDIR "not a directory"
              :EISDIR "is a directory" :EROFS "read-only" :EIO "i/o error"})

(fn new-ninep [world] {:world world :qid-paths {} :qid-next 0 :msize 65536})

(fn isdir? [np path] (= (. (io*.stat np.world path) :kind) :dir))

(fn qid [np path]
  ;; qid[13] = type[1] version[4]=0 path[8]（路徑字串延遲分配穩定整數）
  (when (= (. np.qid-paths path) nil)
    (tset np.qid-paths path np.qid-next)
    (set np.qid-next (+ np.qid-next 1)))
  (let [qtype (if (isdir? np path) 0x80 0)]
    (.. (c.p1 qtype) (c.p4 0) (c.p8 (. np.qid-paths path)))))

(fn base-name [path]
  (var last "unipath")
  (each [seg (string.gmatch path "[^/]+")] (set last seg))
  last)

(fn child-path [path w] (if (= path "/") (.. "/" w) (.. path "/" w)))

(fn errsym [msg]
  ;; 從 pcall 錯誤字串尾端取出 errno 符號（如 ENOENT）
  (or (and (= (type msg) :string) (string.match msg "(%u%u+)%s*$")) "EIO"))

(fn err [name] (values R.error (c.ps (or (. ERRNO name) name))))

(fn stat-bytes [np path]
  ;; type[2] dev[4] qid[13] mode[4] atime[4] mtime[4] length[8] name uid gid muid
  (let [st (io*.stat np.world path)
        isdir (= st.kind :dir)
        mode (if isdir (+ 0x80000000 0x1ED) (if st.ro 0x124 0x1A4))
        len (if isdir 0 st.size)
        inner (.. (c.p2 0) (c.p4 0) (qid np path) (c.p4 mode)
                  (c.p4 0) (c.p4 0) (c.p8 len)
                  (c.ps (base-name path)) (c.ps "unipath")
                  (c.ps "unipath") (c.ps "unipath"))]
    (.. (c.p2 (length inner)) inner)))     ; 內層自帶 size[2]

{: T : R : new-ninep : isdir? : qid : child-path : errsym : err : stat-bytes}
