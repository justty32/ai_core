;; example.fnl — 從 Fennel 用 cllm（lua 5.4 的 llm.so）：基本、串流、schema+JSON(dkjson)、shell。
;; 跑：source ~/repo/dev/env.sh 後  fennel example.fnl "$CLLM_FIXTURES"
(local llm (require :llm))
(local json (require :dkjson))
(local base (or (. arg 1) ""))
(fn ep [n] (.. base n))

(print (.. "[fnl] ask => " (llm.ask "你好" (ep "fake/chat/completions"))))
(io.write "[fnl] 串流 => ")
(llm.ask {:prompt "數到五" :endpoint (ep "fake_stream/chat/completions") :stream true
          :on_delta (fn [p] (io.write "[" p "]") false)})
(print)

;; schema → JSON → dkjson
(local raw (llm.ask {:prompt "給我角色" :endpoint (ep "fake_json/chat/completions") :schema "{\"type\":\"object\"}"}))
(local o (json.decode raw))
(print (string.format "[fnl] json => name=%s affection=%s lines=%d" (. o :name) (tostring (. o :affection)) (length (. o :lines))))

;; shell 呼叫
(local h (io.popen (.. "llm 你好 --endpoint '" (ep "fake/chat/completions") "'")))
(print (.. "[fnl] shell(llm) => " (: (or (h:read "*a") "") :gsub "%s+$" "")))
(h:close)
