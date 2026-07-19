#!/usr/bin/env janet
# bin/main.janet —— CLI 進入點：讀 endpoint/api-key/model → 呼 (llm/ask) → 印結果 / 分流錯誤。
#
# 跑法（通常經 scripts/up.sh，它會先備好 proxy＋憑證再帶環境變數進來）：
#   janet bin/main.janet 用一句話介紹你自己
#   janet bin/main.janet --stream 寫一首短詩
#   janet bin/main.janet --check                       # 離線自檢：binding 就緒？環境變數齊？
#   APP_ENDPOINT=... APP_API_KEY=... janet bin/main.janet 你好
#
# 環境變數（scripts/up.sh 會設；也可自己給）：
#   APP_ENDPOINT  cllm endpoint（預設本機 proxy）
#   APP_API_KEY   Bearer token
#   APP_MODEL     模型 id（預設 claude-opus-4-8）
(import spork/argparse :as ap)
(import ../janet-try-1/init :as app)

(def default-endpoint "http://127.0.0.1:8787/v1/chat/completions")
(def default-model "claude-opus-4-8")

(defn- env [k dflt] (or (os/getenv k) dflt))

(defn- do-check
  "離線自檢：不觸網，回報 binding 與環境狀態。"
  []
  (print "janet-try-1 自檢")
  (printf "  cllm janet binding：%s"
          (if (app/binding-ready?) "就緒（(import llm) OK）"
            "尚未就緒 —— 由平行 agent 裝好、source ~/dev/cllm/env.sh 後再試"))
  (printf "  APP_ENDPOINT：%s" (env "APP_ENDPOINT" (string default-endpoint "（預設）")))
  (printf "  APP_MODEL：   %s" (env "APP_MODEL" (string default-model "（預設）")))
  (printf "  APP_API_KEY： %s" (if (os/getenv "APP_API_KEY") "已設（不外洩內容）" "未設 → 會撞 401"))
  (print "  失敗分流自測：")
  (each [m expect] [["後端錯誤 (HTTP 401): Unauthorized" :auth]
                    ["curl: (7) Failed to connect: Connection refused" :sidecar]
                    ["something else broke" :other]]
    (def [kind _] (app/classify-error m))
    (printf "    %-45s → %s %s" m kind (if (= kind expect) "OK" "MISMATCH")))
  (os/exit 0))

(defn main [& _]
  (def res
    (ap/argparse
      "janet-try-1 —— 用 Janet 打 Anthropic API（經 cllm binding → anthropic-proxy）"
      "stream"   {:kind :flag :short "s" :help "串流輸出（逐段印）。"}
      "check"    {:kind :flag :help "離線自檢（不觸網），看 binding／環境是否備妥。"}
      "endpoint" {:kind :option :help "覆寫 endpoint（預設 $APP_ENDPOINT 或本機 proxy）。"}
      "model"    {:kind :option :short "m" :help "覆寫模型 id。"}
      :default   {:kind :accumulate :help "prompt 各詞（會以空白併回一句）。"}))

  (unless res (os/exit 1))
  (when (res "check") (do-check))

  (def prompt (string/join (or (res :default) @[]) " "))
  (when (empty? prompt)
    (eprint "沒給 prompt。範例：janet bin/main.janet 你好   （或 --check 自檢）")
    (os/exit 2))

  (def endpoint (or (res "endpoint") (env "APP_ENDPOINT" default-endpoint)))
  (def model    (or (res "model")    (env "APP_MODEL" default-model)))
  (def api-key  (os/getenv "APP_API_KEY"))
  (def stream?  (res "stream"))

  (def opts
    @{:endpoint endpoint :api-key api-key :model model :stream stream?
      :on-delta (fn [piece] (prin piece) (flush))})

  (def r (app/ask prompt opts))
  (cond
    (r :ok)
    (do (unless stream? (print (r :text)))
        (when stream? (print))         # 串流時補個換行收尾
        (os/exit 0))

    # 失敗：印分流訊息到 stderr，退出碼帶語意（1=一般/憑證、3=sidecar、4=no-binding）
    (do
      (eprintf "✗ 失敗（%s）：\n    %s" (r :kind) (r :error))
      (os/exit (case (r :kind) :sidecar 3 :no-binding 4 1)))))
