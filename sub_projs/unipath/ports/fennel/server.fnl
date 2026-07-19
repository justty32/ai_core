;; server.fnl —— wire 收發迴圈 + accept 迴圈（對照 unipath_9p.py 的 serve_conn / serve）。
;; 訊息框架：size[4] type[1] tag[2] body。逐筆同步處理（一次一條連線，夠 selftest 用）。

(local c (require :codec))
(local env (require :env))
(local d (require :dispatch))
(local ninep (require :ninep))
(local sock (require :fsock))

(fn serve-conn [np fd]
  (let [fids {}]
    (var alive true)
    (while alive
      (let [hdr (sock.recv-exact fd 4)]
        (if (not hdr) (set alive false)
            (let [total (c.u4 (c.new-reader hdr))   ; size[4]
                  body (sock.recv-exact fd (- total 4))]
              (if (not body) (set alive false)
                  (let [mtype (string.byte body 1)
                        tag (+ (string.byte body 2) (* 256 (string.byte body 3)))
                        r (c.new-reader body)]
                    (set r.i 4)            ; 跳過 type[1]+tag[2]
                    (let [(rtype rbody)
                          (let [(ok a b) (pcall d.dispatch np mtype tag r fids)]
                            (if ok (values a b) (values ninep.R.error (c.ps "i/o error"))))
                          msg (.. (c.p4 (+ 4 1 2 (length rbody)))
                                  (c.p1 rtype) (c.p2 tag) rbody)]
                      (sock.send-all fd msg)))))))))
  (sock.close fd))

(fn serve [port]
  (let [np (ninep.new-ninep (env.build-world))
        srv (sock.make-server port)]
    (io.write (.. "unipath-9p(fennel) 監聽 127.0.0.1:" port "（真 9P2000）\n"))
    (io.flush)
    (while true
      (let [conn (sock.accept srv)]
        (when (>= (tonumber conn) 0) (serve-conn np conn))))))

{: serve}
