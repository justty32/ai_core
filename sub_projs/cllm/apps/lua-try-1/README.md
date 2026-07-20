# lua-try-1 —— 用 Lua 打 Anthropic API 的最小 cllm 應用

一支用 **Lua** 寫、透過 **cllm** 打 **Anthropic API** 的最小應用，整合 cllm 的兩支周邊工具
（**anthropic-proxy** 轉發代理 + **llm-login** 帳號登入）。是姊妹專案 `janet-try-1` 的 Lua 版，
架構／佈線／腳本一比照，只是 app 碼換 Lua。

## 架構

```
  lua app  ──require("llm")──▶  cllm binding (llm.so → libcllm.so)  ──OpenAI+Bearer──▶  anthropic-proxy
  main.lua                     (llm.ask{...})                          127.0.0.1:8787    (sidecar，翻 wire format)
                                                                                               │ x-api-key
                                                                                               ▼
                                                                                      api.anthropic.com
  憑證來源：llm-login（OAuth 帳號登入，僅 OpenRouter 那條）或直接貼 sk-ant key
```

- **cllm** 對外只認「`endpoint` + `Bearer api_key`」。Anthropic 收的是 `x-api-key` + Messages 格式，
  兩邊對不上，所以 **anthropic-proxy** 坐在 endpoint 上當本機 sidecar 翻譯。
- app 本身不碰這些細節——它只 `require("llm")` 呼 `llm.ask{...}`，endpoint 指向本機 proxy。
- **OpenRouter 模式不需 proxy**：OpenRouter 一個 OpenAI-compat endpoint 通吃含 Claude，endpoint 直指官方。

## 結構

```
main.lua                    CLI 進入點（簡易參數解析）：讀 endpoint/key/model → llm.ask → 印/分流
src/app.lua                 核心模組：binding 解析 + 失敗分流（401 vs connection refused）
test/basic.lua              離線測（不觸網、不需 binding）：驗失敗分流
config/config.example.json  cllm config 範本（proxy 路徑；填 api_key＝你的 sk-ant-...）
scripts/up.sh               一鍵佈線：掛環境 + 起 proxy sidecar + 備憑證 + 跑 app
scripts/vendor.sh           把 cllm tools 的 build 產物 vendored 進 vendor/cllm-tools/
vendor/cllm-tools/          vendored 二進位（vendor.sh 產出，gitignored）
```

## 前置（cllm Lua binding 已裝好，不用自己做）

cllm 的 Lua binding（`llm.so`）與 `dkjson.lua` 已裝在 `~/dev/lib/lua/{5.4,5.5}/` 與 `~/dev/share/lua/`，
靠環境變數 `LUA_CPATH_5_4`／`LUA_CPATH_5_5`／`LUA_PATH_5_x` 掛上（見 `~/dev/cllm/env.sh`）。
本機 `lua` 為 5.5（env 同時備了 5.4／5.5 路徑，兩版皆可 `require("llm")`）。

```sh
source ~/dev/cllm/env.sh          # 掛上 LUA_CPATH / LUA_PATH（up.sh 也會自動 source）
```

> tools（anthropic-proxy／llm-login）**已被姊妹 agent 用 cmake 建好**，產物在
> `~/repo/ai_core/sub_projs/cllm/core/build/tools/`。up.sh 直接重用，**不用重跑 cmake**。
> 要與 repo 解耦、離線起 sidecar，跑 `./scripts/vendor.sh` 把產物複製進 `vendor/cllm-tools/`。

## 怎麼跑

### 1. 離線自檢（不觸網）

```sh
lua main.lua --check     # 看 binding 就緒？endpoint/key 齊？失敗分流對不對
lua test/basic.lua       # 離線單元測試（分流邏輯全綠）
```

### 2. 實跑（需憑證）

```sh
# MODE=anthropic（預設）：起 proxy sidecar + 用 sk-ant key
cp config/config.example.json config/config.json    # 改 api_key＝你的 sk-ant-...
#   或 export ANTHROPIC_API_KEY=sk-ant-...
./scripts/up.sh 用一句話介紹你自己
./scripts/up.sh --stream 寫一首關於轉發代理的短詩

# MODE=openrouter：真・OAuth 帳號登入且觸及 Claude（見下節）
MODE=openrouter ./scripts/up.sh 你好
```

`up.sh` 做四件事：① 掛上 cllm Lua 環境（`LUA_CPATH`）② 確保 proxy sidecar 在跑（TCP health-check，
僅 anthropic 模式）③ 備好憑證（llm-login 或 sk-ant key）④ 帶 `APP_ENDPOINT/APP_API_KEY/APP_MODEL`
進來跑 Lua app。

## 失敗怎麼分辨（照 INTEGRATION.md）

app 把 cllm 的錯誤分三類、退出碼帶語意：

| 症狀 | 類別 | 退出碼 | 意思 / 怎麼辦 |
|--|--|--|--|
| HTTP **401** / authentication / 缺 Authorization | `auth` | 1 | **憑證問題**：沒登入或 key 失效 → 填 sk-ant key，或 `llm-login login` |
| **connection refused** | `sidecar` | 3 | **sidecar 沒起**：proxy 沒跑 → `up.sh` 會自動起 |
| `require("llm")` 找不到 | `no-binding` | 4 | binding 尚未就緒 → `source ~/dev/cllm/env.sh` 掛上 `LUA_CPATH` |
| 其他 | `other` | 1 | 上游其他錯誤 |

> 關鍵：`401` ⇒ 憑證問題；`connection refused` ⇒ sidecar 沒跑。兩者不同源，別當成「模型沒回應」。
>
> 實作面：cllm Lua binding 的 `llm.ask{...}` 成功回文字字串；後端錯誤回 `nil, errmsg`
> （`errmsg` 已含「後端錯誤 (HTTP 401): ...」這類可分流字串）；Lua 回呼自己拋錯才會 raise
> （`src/app.lua` 用 `pcall` 包住）。三種情況都收斂進 `app.ask` 的分流。

## Anthropic 帳號登入（OAuth）的現實 ⚠

**「打 Anthropic API」與「OAuth 帳號登入」目前有張力，請先決定要哪條：**

- **Anthropic 直連 API 沒有程式化 OAuth 帳號登入**。它只發 `sk-ant-...` 金鑰、收 `x-api-key`。
  `llm-login` 的 `providers/` **沒有 anthropic**（只有 openrouter / google-gemini / azure-openai /
  github-models）。所以 `MODE=anthropic` 這條的憑證是**你自己貼 sk-ant key**，**不是** OAuth 帳號登入。
- **真要 OAuth 帳號登入、又要用到 Claude**：走 **OpenRouter**。`llm-login` 有現成 openrouter preset
  （OAuth PKCE、免註冊 client、換回不過期 key），OpenRouter 一個 OpenAI-compat endpoint 通吃含 Claude，
  **不需 proxy**。`MODE=openrouter ./scripts/up.sh` 就是這條。

要做 OpenRouter OAuth 帳號登入（**需你親自開瀏覽器一次**）：

```sh
cp ~/repo/ai_core/sub_projs/cllm/core/tools/llm-login/providers/openrouter.json ~/.config/llm/oauth.json
llm-login login        # 開瀏覽器帳號登入 → 換不過期 key
MODE=openrouter ./scripts/up.sh 你好
```

## 需要你（使用者）親自做的

1. **決定憑證路線**：貼 `sk-ant-...`（直連 Anthropic，非 OAuth）vs OpenRouter OAuth 帳號登入（觸及 Claude）。
2. **真 OAuth 往返要你開瀏覽器**：`llm-login login` 屬 WAIT_USER，無人環境跑不完。
3. **填憑證**：`config/config.json` 的 `api_key`，或 `export ANTHROPIC_API_KEY=...`。
4. **確認 Lua binding 就緒**：`source ~/dev/cllm/env.sh` 後 `lua main.lua --check` 應顯示「binding 就緒」。
