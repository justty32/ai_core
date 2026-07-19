;; main.fnl —— 入口：解析 argv，啟動 server。
;; 用法： luajit main.lua serve <port>

(local server (require :server))

(let [cmd (. arg 1) port (tonumber (. arg 2))]
  (if (and (= cmd "serve") port)
      (server.serve port)
      (do (io.stderr:write "用法: luajit main.lua serve <port>\n")
          (os.exit 1))))
