;; dispatch.fnl —— 逐訊息分派：對照 unipath_9p.py 的 dispatch()（每個 T... 一段）。

(local c (require :codec))
(local io* (require :envio))
(local n (require :ninep))
(local T n.T) (local R n.R)

(fn do-walk [np r fids]
  (let [fid (c.u4 r) newfid (c.u4 r) nw (c.u2 r) names []]
    (for [_ 1 nw] (table.insert names (c.rs r)))
    (if (not (. fids fid)) (n.err :ENOENT)
        (let [qids []]
          (var path (. (. fids fid) :path))
          (var stop false)
          (each [_ w (ipairs names) &until stop]
            (let [cand (if (= w "..") "/" (n.child-path path w))
                  ok (pcall #(io*.stat np.world cand))]
              (if ok (do (set path cand) (table.insert qids (n.qid np path)))
                  (set stop true))))
          (if (and (not= (length qids) nw) (= (length qids) 0)) (n.err :ENOENT)
              (do (when (= (length qids) nw)
                    (tset fids newfid {:path path :open false}))
                  (values R.walk (.. (c.p2 (length qids)) (table.concat qids)))))))))

(fn do-read [np r fids]
  (let [fid (c.u4 r) off (c.u8 r) count (c.u4 r) st (. fids fid)]
    (if (not st) (n.err :ENOENT)
        st.dir
        (let []                          ; 目錄：串接預渲染 stat blob
          (var outs "") (var pos 0) (var stop false)
          (each [_ e (ipairs st.dir) &until stop]
            (if (< pos off) (set pos (+ pos (length e)))
                (> (+ (length outs) (length e)) count) (set stop true)
                (do (set outs (.. outs e)) (set pos (+ pos (length e))))))
          (values R.read (.. (c.p4 (length outs)) outs)))
        (let [(ok data) (pcall #(io*.read np.world st.path))]
          (if (not ok) (n.err (n.errsym data))
              (let [chunk (string.sub data (+ off 1) (+ off count))]
                (values R.read (.. (c.p4 (length chunk)) chunk))))))))

(fn dispatch [np mtype tag r fids]
  (if
    (= mtype T.version)
    (let [ms (c.u4 r) ver (c.rs r)]
      (set np.msize (math.min np.msize ms))
      (values R.version (.. (c.p4 np.msize)
        (c.ps (if (= (string.sub ver 1 6) "9P2000") "9P2000" "unknown")))))

    (= mtype T.attach)
    (let [fid (c.u4 r)]
      (c.u4 r) (c.rs r) (c.rs r)          ; afid uname aname
      (tset fids fid {:path "/" :open false})
      (values R.attach (n.qid np "/")))

    (= mtype T.walk) (do-walk np r fids)

    (= mtype T.open)
    (let [fid (c.u4 r) _ (c.u1 r) st (. fids fid)]
      (if (not st) (n.err :ENOENT)
          (do (set st.open true)
              (when (n.isdir? np st.path)
                (set st.dir [])
                (each [_ ch (ipairs (io*.readdir np.world st.path))]
                  (table.insert st.dir (n.stat-bytes np (n.child-path st.path ch)))))
              (values R.open (.. (n.qid np st.path) (c.p4 0))))))

    (= mtype T.read) (do-read np r fids)

    (= mtype T.write)
    (let [fid (c.u4 r) _ (c.u8 r) count (c.u4 r)
          data (c.rbytes r count) st (. fids fid)]
      (if (not st) (n.err :ENOENT)
          (let [(ok res) (pcall #(io*.write np.world st.path data))]
            (if (not ok) (n.err (n.errsym res)) (values R.write (c.p4 res))))))

    (= mtype T.stat)
    (let [fid (c.u4 r) st (. fids fid)]
      (if (not st) (n.err :ENOENT)
          (let [sb (n.stat-bytes np st.path)]
            (values R.stat (.. (c.p2 (length sb)) sb)))))   ; 雙重 size

    (= mtype T.clunk)
    (let [fid (c.u4 r)] (tset fids fid nil) (values R.clunk ""))

    (= mtype T.flush) (do (c.u2 r) (values R.flush ""))

    (or (= mtype T.create) (= mtype T.remove) (= mtype T.wstat)) (n.err :EROFS)

    (n.err :EIO)))

{: dispatch}
