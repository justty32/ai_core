# output.janet — llm/ask 的四個出口回呼打包成 Sink（對齊 core/src/cli_output.cpp 的 Sink）。
#
# 把「輸出接線 + 其共享狀態」收成一個物件：文字吐 stdout、tool_calls 一行一則 JSON、產出媒體落檔
# --media-out、錯誤吐 stderr。回呼一律回 false（不主動中止）。收尾狀態（wrote-text/had-error/
# media-err）由 main.janet 讀來定退出碼。

(import spork/json)
(import ./media :as media)

(defn make-sink [media-out-dir]
  ``建 sink：回 struct {:state 共享狀態 :on-delta :on-tool :on-media :on-error 綁定回呼}。
  把 sink 的 :on-* 傳給 llm/ask 的對應關鍵字。``
  (def state @{:wrote-text false :had-error false :media-err false :media-n 0})

  (defn on-delta [piece] # 文字吐 stdout
    (prin piece)
    (flush)
    (put state :wrote-text true)
    false)

  (defn on-tool [call] # tool_calls 一行一則 JSON：{"tool","id","arguments"}
    # arguments 原樣內嵌（binding 交來的是 JSON 字串，直接嵌以保留原始 UTF-8，不經 json/encode 轉義）。
    (def args (call :arguments))
    (def args-json (if (or (nil? args) (empty? args)) "{}" args))
    (print (string "{\"tool\":" (json/encode (call :name))
                   ",\"id\":" (json/encode (call :id))
                   ",\"arguments\":" args-json "}"))
    (flush)
    false)

  (defn on-media [m] # 產出媒體落檔 --media-out；沒目錄＝明說丟棄
    (if (nil? media-out-dir)
      (do
        (eprintf "收到產出媒體（%s，%d bytes）但沒給 --media-out，已丟棄"
                 (m :mime) (length (m :bytes)))
        false)
      (do
        (put state :media-n (inc (state :media-n)))
        (def path (string media-out-dir "/llm-media-" (state :media-n) "." (media/ext-of (m :mime))))
        (def f (file/open path :wb))
        (if (nil? f)
          (do (eprintf "媒體落檔失敗：%s" path) (put state :media-err true) false)
          (do
            (file/write f (m :bytes))
            (file/close f)
            (print path)
            (flush)
            false)))))

  (defn on-error [msg] # 錯誤吐 stderr；標記請求真失敗
    (put state :had-error true)
    (eprintf "請求失敗：%s" msg)
    nil)

  {:state state :on-delta on-delta :on-tool on-tool :on-media on-media :on-error on-error})
