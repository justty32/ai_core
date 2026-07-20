#!/usr/bin/env janet
# example.janet — cllm Janet binding：基本 ask、串流、schema+JSON(spork/json)、tools、
#   media、modalities、shell(CLI) 呼叫。
# 跑：source ~/dev/cllm/env.sh 後  janet example.janet "$CLLM_FIXTURES"
#   （或裝好後直接 JANET_PATH 指到 llm.so 所在目錄）
(import llm)
(import spork/json)
(import spork/sh)

(def base (get (dyn :args) 1 ""))
(defn ep [n] (string base n))

(print (string "[janet] ask => " (llm/ask "你好" (ep "fake/chat/completions"))))

(prin "[janet] 串流 => ")
(llm/ask "數到五" :endpoint (ep "fake_stream/chat/completions") :stream true
         :on-delta (fn [p] (prin "[" p "]") false))
(print)

# schema → JSON → spork/json 解析
(def raw (llm/ask "給我角色" :endpoint (ep "fake_json/chat/completions") :schema "{\"type\":\"object\"}"))
(def o (json/decode raw))
(printf "[janet] json => name=%s affection=%d lines=%d" (o "name") (o "affection") (length (o "lines")))

# tools + on-tool：fake_tool fixture 固定回一個 tool_call；arguments 是 JSON 字串，用 spork/json 解
(llm/ask "東京天氣如何？" :endpoint (ep "fake_tool/chat/completions")
         :tools [{:name "get_weather" :description "查詢城市天氣"
                  :parameters "{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"},\"unit\":{\"type\":\"string\"}}}"}]
         :on-tool (fn [call]
                    (def args (json/decode (call :arguments)))
                    (printf "[janet] tool => %s(city=%s, unit=%s)" (call :name) (args "city") (args "unit"))
                    false))

# media 輸出：fake_media fixture 回 audio → on-media 收 mime/bytes
(llm/ask "說句話" :endpoint (ep "fake_media/chat/completions")
         :on-media (fn [m]
                     (printf "[janet] media out => mime=%s bytes=%d" (m :mime) (length (m :bytes)))
                     false))

# media 輸入＋modalities：掛 data URI 圖片＋audio 模態參數打 fake（body 被忽略，驗證搬運不炸）
(def ans (llm/ask "描述這張圖" :endpoint (ep "fake/chat/completions")
                  :media [{:url "data:image/png;base64,iVBORw0KGgo="}]
                  :modalities [{:name "audio" :config "{\"voice\":\"alloy\"}"}]))
(print (string "[janet] media in+modality => " (if ans "ok" "nil")))

# shell 呼叫：從 Janet 呼叫 llm CLI
(def out (string/trimr (sh/exec-slurp "llm" "你好" "--endpoint" (ep "fake/chat/completions"))))
(print (string "[janet] shell(llm) => " out))
