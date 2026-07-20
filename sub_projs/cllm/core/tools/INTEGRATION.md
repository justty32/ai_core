# 把 tools 接進你的東西 —— cllm CLI／lib／AI 應用

← [tools](README.md)｜相關：[anthropic-proxy](anthropic-proxy/README.md)｜[llm-login](llm-login/README.md)

> **一句話**：這兩支都是**行程外**的東西，所以語言無關、binding 無關——cllm 不管是 CLI
> 還是哪個語言的 lib，對外都只是「打 HTTP 到 `endpoint`＋帶 `Bearer api_key`」。接法對所有
> binding 一模一樣，**沒有 per-language 工**。

## 兩支各站在哪一層

```
   你的東西（cllm CLI／某語言 lib binding／AI 應用）
        │  只認：endpoint + Bearer api_key
        ▼
   ┌─ anthropic-proxy ─┐   坐在 endpoint 上的 HTTP sidecar（翻 wire format）——只有連 Anthropic 才需要
   └───────────────────┘
        │
        ▼  llm-login：旁邊產出／續期 api_key（有 OAuth 的廠商）——不在請求路徑上
   廠商 API（Anthropic／OpenRouter／DeepSeek…）
```

- **anthropic-proxy**：只有目標是 **Anthropic 直連**時才要（cllm 說 OpenAI＋Bearer，Anthropic 收
  x-api-key＋Messages，代理翻譯）。其餘 OpenAI-compat 廠商（DeepSeek 直填、OpenRouter…）**不需要它**。
- **llm-login**：只有廠商**有程式化 OAuth**（OpenRouter／Gemini／Azure／GitHub Models）才用；它把
  token patch 進 cllm config 的 `api_key`。DeepSeek 這種只發 API key 的**直接填 key**，兩支都不用。

## 情境 A：cllm lib＋某語言 binding 的開發環境

binding 再怎麼換語言，也只要 `endpoint`＋`api_key` 兩個值。所以開發環境要做的只是「把這兩個值餵給 binding」：

| 目標廠商 | 開發環境要備什麼 |
|--|--|
| Anthropic | proxyctl／systemd 起 proxy 當 sidecar → binding 指 `127.0.0.1:8787`＋填 sk-ant |
| DeepSeek | 不用任何工具 → binding 直接指 `api.deepseek.com`＋填 sk- |
| OpenRouter／Gemini/… | 先 `llm_login login` 產憑證 → binding 指官方 endpoint＋用產出的 key |

**建議**：給 binding 專案一個 `up` 動作（Makefile／justfile／shell target），做兩件事：①確保 proxy
在跑（若需要）②把 endpoint/key 設給 binding（env 或寫 binding 自己的 config）。binding 換語言時，只有
「怎麼把兩個值餵進去」這一小段變，sidecar 那半完全不動。

## 情境 B：把工具嵌進 AI 應用

三種整合法，**推薦第 1 種**：

1. **（推薦）Vendored sidecar** —— 把 `anthropic-proxy/`（和需要時 `llm-login/`）**整包 copy** 進 app
   （例如 `vendor/cllm-tools/`）。因為**零外部相依、純 Python 3.11 stdlib**，搬過去就能跑、不用打包不用裝依賴。
   app 啟動時把 `proxy.py` **spawn 成子行程**（綁 ephemeral port）→ cllm client 指過去；要新 vendor
   token 時 subprocess 呼 `llm_login.py token`。**無 import 耦合、無語言綁定**。
2. **git submodule／pinned copy** —— 想讓上游更新流進 app 時用。
3. **不推薦：source-merge** —— 把 `.py` import 進 app 的模組。這會耦合它們的內部；這兩支是**設計成獨立
   行程**的，當黑箱行程用最穩。

> **為什麼「搬資料夾」在這裡是對的**：零相依＋純 stdlib ⇒ `copy + spawn subprocess` 就是乾淨解。
> 換作有一堆 pip 依賴的東西才需要煩打包；這裡不用。

## 失敗長怎樣（沒登入／key 失效／proxy 沒起）—— app 必須處理

cllm **不會卡住、不會靜默回空成功**。憑證出問題時走 `cabi_response.cpp` 的 `guard_backend_error`：
HTTP≥400 或回應含 `{"error":{"message"}}` 就 `throw` → `llm_ask` catch → 觸發 `on_error`、phase 設
ERROR、回傳 `LLM_ERROR`（**CLI** 印 `後端錯誤 (HTTP 4xx): <訊息>` 到 stderr 並非零退出；**lib** 是
`on_error` callback 拿到同一段字串＋回傳 `LLM_ERROR`，不會有例外穿過 C ABI）。

| 情況 | 症狀 | 怎麼分辨 |
|--|--|--|
| config 沒填 key（空/placeholder） | cllm 不送 Authorization → 代理/廠商回 **401** | `authentication_error`／「缺 Authorization」|
| key 失效／過期 | 上游回 **401**（proxy 原樣翻回 OpenAI 錯誤外殼） | 同上 |
| llm-login 沒登入 | `llm_login.py token` stdout 空、退出 1 → 等同沒填 key → **401**（過期但有 refresh_token 會自動續，不報錯）| 同上 |
| **proxy／sidecar 沒起** | 連 `127.0.0.1:<port>` 被拒 → **connection refused**（不是 401）| 連線錯誤 ≠ 憑證錯誤 |

**要點**：`401` ⇒ 憑證問題（沒登入／key 壞）；`connection refused` ⇒ sidecar 沒跑。app 嵌入時應
（a）啟動先 health-check proxy、（b）把 cllm 的 `LLM_ERROR`＋錯誤字串接出來分流這兩類，別當成「模型沒回應」。

## 之後要我做的（等你定「語言＋廠商」再動）

- **proxy 支援 `--port 0`（OS 配埠）＋印出實際綁定的埠** → app spawn 後好讀埠，多實例不撞埠。（小改，嵌入用）
- **一個 embed helper**：選空埠 → 起 proxy → health-check → 寫 binding 的 config → app 退出時收乾淨。
- **針對你選的 binding 的 `up` 腳本／compose sidecar**：一鍵把開發環境拉起來。
- 若走 Vendored sidecar：一支 `vendor.sh`，把該搬的資料夾 copy 進目標 app 並留下版本標記。

> 這些都語言/廠商相關的最後一哩，等情境具體（哪個 binding、哪個廠商）再補，免得空做。
