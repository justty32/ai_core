;; output.fnl — 內核 ask 的四個出口回呼打包成 Sink（對齊 core/src/cli_output.cpp 的 Sink）。
;;
;; 把「輸出接線 + 其共享狀態」收成一個 table：文字吐 stdout、tool_calls 一行一則 JSON、產出媒體
;; 落檔 --media-out、錯誤吐 stderr。回呼一律回 false（不主動中止）。收尾狀態（wrote_text/had_error/
;; media_err）由 cli.fnl 讀來定退出碼。回呼是閉包（llm.ask 以純函式呼叫，非方法），故綁 opts 的
;; on_delta/on_tool/on_media/on_error（底線鍵）即可。

(local json (require :dkjson))
(local media (require :media))

(fn make-sink [media-out-dir]
  "回一個 sink：{on_delta on_tool on_media on_error} ＋收尾狀態欄位。"
  (local s {:media-out-dir media-out-dir
            :wrote-text false :had-error false :media-err false :media-n 0})

  (set s.on_delta
    (fn [piece] ; 文字（串流逐段／非串流整段皆走此）→ stdout
      (io.write piece) (io.flush) (set s.wrote-text true) false))

  (set s.on_tool
    (fn [call] ; tool_calls 一行一則 JSON：{"tool","id","arguments"}（arguments 原樣內嵌）
      (let [(obj _ jerr) (json.decode (or call.arguments "{}"))
            args (if obj obj (or call.arguments "{}")) ; 壞 JSON 就原樣塞字串
            line (json.encode {:tool call.name :id call.id :arguments args}
                              {:keyorder ["tool" "id" "arguments"]})]
        (io.write line "\n") (io.flush) false)))

  (set s.on_media
    (fn [m] ; 產出媒體落檔 --media-out；沒給＝明說丟棄（不無聲吞）
      (if (not s.media-out-dir)
          (do (io.stderr:write (string.format "收到產出媒體（%s，%d bytes）但沒給 --media-out，已丟棄\n"
                                              m.mime (length m.bytes)))
              false)
          (do
            (set s.media-n (+ s.media-n 1))
            (let [path (.. s.media-out-dir "/llm-media-" s.media-n "." (media.ext-of m.mime))
                  (f oerr) (io.open path :wb)]
              (if (not f)
                  (do (io.stderr:write (.. "媒體落檔失敗：" path "\n")) (set s.media-err true) false)
                  (do (f:write m.bytes) (f:close) (io.write path "\n") (io.flush) false)))))))

  (set s.on_error
    (fn [msg] ; 請求失敗 → stderr（帶前綴）；並戳 had-error 供定退出碼
      (set s.had-error true)
      (io.stderr:write (.. "請求失敗：" msg "\n"))
      false))
  s)

{: make-sink}
