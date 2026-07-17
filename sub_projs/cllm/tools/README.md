# tools — cllm 周邊工具（非核心、不進主建置）

← [cllm INDEX](../INDEX.md)

建在 cllm 之上、但**不屬 C/C++ 核心**的輔助工具。核心（`src/`）維持純 C++23；這裡放
「還在試」、一次性或 dev 便利性質的東西，語言不限（登入這類一次性動作用零相依 Python）。

| 工具 | 是什麼 |
|------|--------|
| [llm-login/](llm-login/README.md) | 用 OAuth（授權碼＋PKCE）**帳號登入**換 token 餵給 `llm` CLI 的 `api_key`；到期自動 refresh。cllm 核心不動——補上它缺的「登入」那段。零外部相依（Python 3.11+ 標準庫）|
| [anthropic-proxy/](anthropic-proxy/README.md) | 本機**轉發代理**：cllm 說 OpenAI＋Bearer、Anthropic 收 x-api-key＋Messages，中間翻譯把兩邊接起來，讓 cllm 直連 `api.anthropic.com`。無狀態、金鑰轉手不落地。補上 llm-login 明說「接不了」的 Anthropic 直連。零外部相依（Python 3.11+ 標準庫）|

## 把工具接進你的東西

- [INTEGRATION.md](INTEGRATION.md) — 這兩支怎麼接進 cllm CLI／某語言的 lib binding／你的 AI 應用。核心：**兩支都在行程外**，所以 binding 無關；含嵌入法（推薦 vendored sidecar）＋失敗行為（沒登入/key 失效/proxy 沒起 各長怎樣）。
