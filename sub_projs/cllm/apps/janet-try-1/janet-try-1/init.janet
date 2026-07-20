# janet-try-1/init.janet —— 核心模組（純邏輯，別人 import 這個）。
#
# 只做兩件事：
#   1. 動態解析 cllm 的 janet binding（(import llm) 提供的 (llm/ask ...)），
#      即使 binding 尚未就緒也不會編譯期爆掉——讓 --help / --check 能離線跑。
#   2. 呼 llm/ask，並把失敗照 tools/INTEGRATION.md 分流：
#         401 / authentication  → 憑證問題（沒登入 / key 失效）→ 要登入或填 key
#         connection refused    → sidecar 沒起（proxy 沒跑）
#      這兩類必須分清楚，別當成「模型沒回應」。

# ── binding 解析 ──────────────────────────────────────────────
# 平行 agent 會把 cllm 的 janet binding 裝成可 (import llm) 的模組（掛在 JANET_PATH，
# 透過 ~/dev/cllm/env.sh）。我們用 require 動態載，避免「模組還沒好」時整支編不過。
# require 回傳模組 env（symbol → {:value ...}）；import 時才會加 llm/ 前綴，故這裡查裸名 'ask。
(defn resolve-ask
  "回傳 cllm binding 的 ask 函式；binding 未就緒則回 nil。"
  []
  (try
    (let [m (require "llm")]
      (or (get-in m ['ask :value])
          (get-in m ['llm/ask :value])))
    ([_err] nil)))

(defn binding-ready?
  "cllm janet binding 是否已可 import。"
  []
  (not (nil? (resolve-ask))))

# ── 失敗分流 ──────────────────────────────────────────────────
(defn classify-error
  "把 llm/ask 的錯誤訊息分成三類：:auth / :sidecar / :other。回 [類別 人話說明]。"
  [msg]
  (def m (string/ascii-lower (or msg "")))
  (cond
    (or (string/find "401" m)
        (string/find "authentication" m)
        (string/find "unauthorized" m)
        (string/find "authorization" m)
        (string/find "未登入" (or msg ""))
        (string/find "token 失效" (or msg "")))
    [:auth (string "憑證問題（HTTP 401 / 未授權）：沒登入或 api_key 失效。\n"
                   "    → Anthropic 直連：確認 config 的 api_key 是有效的 sk-ant-... 金鑰。\n"
                   "    → 若走 OpenRouter OAuth：跑 `llm-login login` 重新帳號登入。")]

    (or (string/find "connection refused" m)
        (string/find "couldn't connect" m)
        (string/find "could not connect" m)
        (string/find "connection reset" m)
        (string/find "failed to connect" m))
    [:sidecar (string "連線被拒（connection refused）：anthropic-proxy sidecar 沒在跑。\n"
                      "    → 先起 proxy：scripts/up.sh 會自動起，或手動 proxyctl.sh start。")]

    [:other (string "其他錯誤：" (or msg "unknown"))]))

# ── 對外主呼叫 ────────────────────────────────────────────────
(defn ask
  ``問一次 LLM。opts 是 table/struct：
     :endpoint  cllm endpoint（proxy 或廠商官方）
     :api-key   Bearer token（sk-ant-... 或 OAuth 換來的 key）
     :model     模型 id（如 claude-opus-4-8）
     :stream    是否串流（真值時逐段進 :on-delta）
     :on-delta  (fn [piece])  串流片段
   回傳 {:ok true :text ...} 或 {:ok false :kind :auth/:sidecar/:other :error 說明}。
   完全不觸網、不需 binding 就能被單元測試 classify-error；真呼叫才需要 binding。``
  [prompt opts]
  (def ask-fn (resolve-ask))
  (if (nil? ask-fn)
    {:ok false :kind :no-binding
     :error (string "cllm 的 janet binding 尚未就緒：(import llm) 找不到模組。\n"
                    "    → 由平行 agent 裝好、`source ~/dev/cllm/env.sh` 掛上 JANET_PATH 後再跑。")}
    (do
      (var captured nil)
      # 對齊 python binding：ask(prompt, & kwopts)，含 :endpoint/:api-key/:model/:stream/:on-delta/:on-error。
      (def kw
        [:endpoint  (get opts :endpoint)
         :api-key   (get opts :api-key)
         :model     (get opts :model)
         :stream    (get opts :stream)
         :on-delta  (get opts :on-delta)
         :on-error  (fn [m] (set captured m))])
      (def result
        (try
          (ask-fn prompt ;kw)
          ([err] (set captured (string err)) nil)))
      (if (and result (nil? captured))
        {:ok true :text result}
        (let [[kind explain] (classify-error captured)]
          {:ok false :kind kind :error explain})))))
