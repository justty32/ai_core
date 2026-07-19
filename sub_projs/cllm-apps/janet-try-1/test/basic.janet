# test/basic.janet —— 離線測（不觸網、不需 binding）：驗失敗分流邏輯。
(import ../janet-try-1/init :as app)

(defn- kind-of [m] (first (app/classify-error m)))

(assert (= :auth    (kind-of "後端錯誤 (HTTP 401): Unauthorized")) "401 → :auth")
(assert (= :auth    (kind-of "authentication_error: invalid x-api-key")) "authentication → :auth")
(assert (= :auth    (kind-of "缺 Authorization")) "authorization → :auth")
(assert (= :sidecar (kind-of "curl: (7) Failed to connect to 127.0.0.1 port 8787: Connection refused")) "refused → :sidecar")
(assert (= :other   (kind-of "model produced garbage")) "unknown → :other")
(assert (= :other   (kind-of nil)) "nil → :other 不爆")

# binding 未就緒時 ask 要走 :no-binding、不觸網
(unless (app/binding-ready?)
  (def r (app/ask "hi" @{:endpoint "http://127.0.0.1:8787/v1/chat/completions"}))
  (assert (= :no-binding (r :kind)) "binding 未就緒 → :no-binding"))

(print "basic.janet：全綠")
