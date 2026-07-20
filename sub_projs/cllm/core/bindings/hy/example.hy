#!/usr/bin/env hy
;; example.hy — cllm Hy binding：基本 ask、串流、schema+JSON 解析、tools、media、
;;   modalities、shell(CLI) 呼叫。輸出標記與 python/example.py 逐字對齊（共用 smoke 標記）。
;; POSIX 跑：source ~/dev/cllm/env.sh 後  hy example.hy "$CLLM_FIXTURES"
;; Windows 跑：pwsh -File ../../../lab/hy/run.ps1（或見 smoke.sh 湊環境）——需先 pip install hy。
(import sys json subprocess)
(import llm)

(setv base (if (> (len sys.argv) 1) (get sys.argv 1) ""))
(defn ep [n] (if base (+ base n) None))

(print "[hy] ask =>" (llm.ask "你好" (ep "fake/chat/completions")))

(print "[hy] 串流 => " :end "")
(llm.ask "數到五" :endpoint (ep "fake_stream/chat/completions") :stream True
         :on-delta (fn [p] (print f"[{p}]" :end "" :flush True) False))
(print)

;; schema → JSON → stdlib json 解析
(setv obj (json.loads (llm.ask "給我角色" :endpoint (ep "fake_json/chat/completions")
                               :schema "{\"type\":\"object\"}")))
(print (.format "[hy] json => name={} affection={} lines={}"
                (get obj "name") (get obj "affection") (len (get obj "lines"))))

;; tools＋on_tool：模型回 tool_call，on-tool 收 call={id,name,arguments}，arguments 是 JSON 字串
(defn handle-tool [call]
  (setv args (json.loads (get call "arguments")))
  (print (.format "[hy] tool => {}(city={}, unit={})"
                  (get call "name") (get args "city") (get args "unit"))))

(llm.ask "東京天氣如何？" :endpoint (ep "fake_tool/chat/completions")
         :tools [{"name" "get_weather" "description" "查某城市天氣"
                  "parameters" (+ "{\"type\":\"object\",\"properties\":{\"city\":{\"type\":\"string\"},"
                                  "\"unit\":{\"type\":\"string\"}}}")}]
         :on-tool handle-tool)

;; media 輸出：模型回 audio，on-media 收 m={mime,bytes}
(defn handle-media [m]
  (print (.format "[hy] media out => mime={} bytes={}"
                  (get m "mime") (len (get m "bytes")))))

(llm.ask "說句話" :endpoint (ep "fake_media/chat/completions") :on-media handle-media)

;; media 輸入＋modalities：掛上 Request 送出（fixture 不看 body，驗的是搬運不炸）
(try
  (llm.ask "描述這張圖" :endpoint (ep "fake/chat/completions")
           :media [{"url" "data:image/png;base64,iVBORw0KGgo="}]
           :modalities [{"name" "audio" "config" "{\"voice\":\"alloy\"}"}])
  (print "[hy] media in+modality => ok")
  (except [e llm.LLMError]
    (print "[hy] media in+modality =>" e)))

;; shell 呼叫：從 Hy 呼叫 llm CLI，捕捉答案
;;   ⚠ 兩個 Windows 坑（對 POSIX 也無害，故不分平台，同 python/play.py）：
;;   ① stdin=DEVNULL：llm 是 unix filter，會讀 inherited stdin；不給 EOF 會卡住等輸入。
;;   ② encoding="utf-8"：llm 輸出 UTF-8，Windows locale 碼頁（繁中 cp950）解碼會炸，須顯式 utf-8。
(setv out (subprocess.run ["llm" "你好" "--endpoint" (ep "fake/chat/completions")]
                          :capture-output True :encoding "utf-8" :stdin subprocess.DEVNULL))
(print "[hy] shell(llm) =>" (.strip out.stdout))
