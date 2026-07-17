# tools — cllm 周邊工具（C++ 模組，進主建置）

← [cllm INDEX](../INDEX.md)

建在 cllm 之上的兩支輔助工具，已從 Python 重寫成**純 C++23、就跟 cllm 一樣的模組形態**：
進主建置（`-DCLLM_BUILD_TOOLS`，預設 ON）、重用 `src/http` 出站、glaze 反射 JSON、零新外部
依賴（入站用自寫微型 server [`common/httpd`](common/httpd.hpp)、PKCE 用 vendored SHA-256）。
Python 原版封存在各工具的 `reference/`（一比一對照，不進建置）。

| 工具 | 是什麼 | 產物 |
|------|--------|------|
| [anthropic-proxy/](anthropic-proxy/README.md) | 本機**轉發代理**：cllm 說 OpenAI＋Bearer、Anthropic 收 x-api-key＋Messages，中間翻譯把兩邊接起來，讓 cllm 直連 `api.anthropic.com`。無狀態、金鑰轉手不落地 | `anthropic-proxy`（執行檔）|
| [llm-login/](llm-login/README.md) | 用 OAuth（授權碼＋PKCE）**帳號登入**換 token 餵給 cllm 的 `api_key`、到期自動 refresh。**C-ABI lib**（`login_abi.h`），為與 cllm 聯動而生：cllm 401 時 harness 呼登入→開瀏覽器→patch config→重試，cllm 零改動 | `liblogin.so`＋`llm-login`（CLI）|

## 共用元件

- [common/httpd.{hpp,cpp}](common/httpd.hpp) — 微型 HTTP/1.1 server（cllm 只有 client；兩支入站監聽共用）。出站則重用 [`../src/http`](../src/http.hpp)。

## 建置

```sh
cmake --build --preset linux-debug          # 隨 cllm 一起建 tools/
# 產物：build/tools/{anthropic-proxy, llm-login, liblogin.so}
```

## 把工具接進你的東西

- [INTEGRATION.md](INTEGRATION.md) — 這兩支怎麼接進 cllm CLI／某語言的 lib binding／你的 AI 應用。核心：**兩支都在行程外**，所以 binding 無關；含嵌入法（推薦 vendored sidecar）＋失敗行為（沒登入/key 失效/proxy 沒起 各長怎樣）。
