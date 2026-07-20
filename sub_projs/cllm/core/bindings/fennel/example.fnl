;; example.fnl — 從 Fennel 用 cllm（lua 5.4 的 llm.so）：基本、串流、schema+JSON(dkjson)、
;;   tools、media、modalities、shell。
;; 跑：source ~/dev/cllm/env.sh 後  fennel example.fnl "$CLLM_FIXTURES"
;; ⚠ 這裡呼叫的是 lua 的 llm binding，table 鍵一律用底線（:on_tool 不是 :on-tool）。
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

;; tools + on_tool：fake_tool fixture 固定回一個 tool_call；arguments 是 JSON 字串，用 dkjson 解
(llm.ask {:prompt "東京天氣如何？" :endpoint (ep "fake_tool/chat/completions")
          :tools [{:name "get_weather" :description "查詢城市天氣"
                   :parameters "{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"},\"unit\":{\"type\":\"string\"}}}"}]
          :on_tool (fn [call]
                     (let [args (json.decode call.arguments)]
                       (print (string.format "[fnl] tool => %s(city=%s, unit=%s)" call.name args.city args.unit)))
                     false)})

;; media 輸出：fake_media fixture 回 audio → on_media 收 mime/bytes
(llm.ask {:prompt "說句話" :endpoint (ep "fake_media/chat/completions")
          :on_media (fn [m]
                      (print (string.format "[fnl] media out => mime=%s bytes=%d" m.mime (length m.bytes)))
                      false)})

;; media 輸入＋modalities：掛 data URI 圖片＋audio 模態參數打 fake（body 被忽略，驗證搬運不炸）
(local (ok err) (llm.ask {:prompt "描述這張圖" :endpoint (ep "fake/chat/completions")
                           :media [{:url "data:image/png;base64,iVBORw0KGgo="}]
                           :modalities [{:name "audio" :config "{\"voice\":\"alloy\"}"}]}))
(print (.. "[fnl] media in+modality => " (if ok "ok" (tostring err))))

;; shell 呼叫
(local h (io.popen (.. "llm 你好 --endpoint '" (ep "fake/chat/completions") "'")))
(print (.. "[fnl] shell(llm) => " (: (or (h:read "*a") "") :gsub "%s+$" "")))
(h:close)
