# tools — cllm 周邊工具（非核心、不進主建置）

← [cllm INDEX](../INDEX.md)

建在 cllm 之上、但**不屬 C/C++ 核心**的輔助工具。核心（`src/`）維持純 C++23；這裡放
「還在試」、一次性或 dev 便利性質的東西，語言不限（登入這類一次性動作用零相依 Python）。

| 工具 | 是什麼 |
|------|--------|
| [llm-login/](llm-login/README.md) | 用 OAuth（授權碼＋PKCE）**帳號登入**換 token 餵給 `llm` CLI 的 `api_key`；到期自動 refresh。cllm 核心不動——補上它缺的「登入」那段。零外部相依（Python 3.11+ 標準庫）|
