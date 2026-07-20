# cpp-try-1 —— 用 C++23 打 Anthropic API 的最小 cllm 應用

一支用 **C++23** 寫、透過 **cllm C++ binding**（`<cllm/llm.hpp>`）打 **Anthropic API** 的最小應用，
整合 cllm 的兩支周邊工具（**anthropic-proxy** 轉發代理 + **llm-login** 帳號登入）。

> 這是姊妹專案 [`cllm-apps/janet-try-1`](../janet-try-1/)（Janet 版，已完成）的 **C++ 版**——
> 架構、佈線、腳本、README 結構全部比照，只把 app 碼換成 C++。

## 架構

```
  cpp app  ──#include <cllm/llm.hpp>──▶  cllm C++ binding (libcllm.so)  ──OpenAI+Bearer──▶  anthropic-proxy
  src/main.cpp    llm::Client::ask()      (pkg-config cllm、rpath 找 .so)   127.0.0.1:8787   (sidecar，翻 wire format)
                                                                                                    │ x-api-key
                                                                                                    ▼
                                                                                           api.anthropic.com
  憑證來源：llm-login（OAuth 帳號登入，僅 OpenRouter 那條）或直接貼 sk-ant key
```

- **cllm** 對外只認「`endpoint` + `Bearer api_key`」。Anthropic 收的是 `x-api-key` + Messages 格式，
  兩邊對不上，所以 **anthropic-proxy** 坐在 endpoint 上當本機 sidecar 翻譯。
- app 本身不碰這些細節——它只 `#include <cllm/llm.hpp>`、`client.ask(prompt)`，endpoint 指向本機 proxy。
- 與 janet 版的差異：C++ binding 是**編譯期連結** libcllm.so（`pkg-config --cflags --libs cllm`），
  不像 janet 是執行期 `(import llm)`；所以沒有「binding 未就緒」這一類——編得過＝binding 就緒。

## 結構

```
build.sh                   一鍵編（pkg-config cllm，與 cllm-lab/cpp/run.sh 同慣例）；build.sh test 另跑離線測
src/app.hpp                核心模組（純邏輯）：ask 包裝 + 失敗分流（classify_error：401 vs connection refused）
src/main.cpp               CLI 進入點：讀 endpoint/key/model → llm::Client::ask / ask_as<T> → 印/分流
test/test_classify.cpp     離線測（不觸網、不需 binding 執行）：驗失敗分流三類 + 退出碼語意
config/config.example.json cllm config 範本（proxy 路徑；填 api_key＝你的 sk-ant-...）
scripts/up.sh              一鍵佈線：起 proxy sidecar + 備憑證 + 跑 app
scripts/vendor.sh          把 cllm tools 的 build 產物 vendored 進 vendor/cllm-tools/
vendor/cllm-tools/         vendored 二進位（vendor.sh 產出，gitignored）
build/                     編譯產物（gitignored）
```

## 怎麼跑

### 0. 前置（一次）

```sh
# a. 掛上 cllm 開發環境（設 PKG_CONFIG_PATH 讓 pkg-config 找到 cllm.pc、LD_LIBRARY_PATH 保險）
source ~/dev/cllm/env.sh

# b. 編 app
cd ~/code/cllm-apps/cpp-try-1 && ./build.sh          # → build/cpp-try-1

# c.（可選）把 tool 產物 vendored 進 app，之後與 repo 解耦
./scripts/vendor.sh
```

> tools（anthropic-proxy／llm-login／liblogin.so）**已由姊妹 agent 用 cmake 建好**，產物在
> `~/repo/ai_core/sub_projs/cllm/build/tools/`。本專案**直接重用**，不重跑 cmake。

### 1. 離線自檢（不觸網）

```sh
./build/cpp-try-1 --check     # 看 endpoint/key 齊不齊、失敗分流對不對
./build.sh test               # 離線單元測試（classify_error 三類 + 退出碼）
```

### 2. 實跑（需憑證）

```sh
# MODE=anthropic（預設）：起 proxy sidecar + 用 sk-ant key
cp config/config.example.json config/config.json    # 改 api_key＝你的 sk-ant-...
#   或 export ANTHROPIC_API_KEY=sk-ant-...
./scripts/up.sh 用一句話介紹你自己
./scripts/up.sh --stream 寫一首關於轉發代理的短詩
./scripts/up.sh --json 給我一個關於轉發代理的冷知識     # 結構化輸出（ask_as<Fact>）

# MODE=openrouter：真・OAuth 帳號登入且觸及 Claude（見下節）
MODE=openrouter ./scripts/up.sh 你好
```

`up.sh` 做三件事：① 確保 app 已編（沒編就跑 build.sh）② 確保 proxy sidecar 在跑（TCP health-check，
僅 anthropic 模式）③ 帶 `APP_ENDPOINT/APP_API_KEY/APP_MODEL` 進來跑 C++ app。

## 失敗怎麼分辨（照 INTEGRATION.md）

app 把 cllm 的錯誤分類、退出碼帶語意（`src/app.hpp::classify_error`）：

| 症狀 | 類別 | 退出碼 | 意思 / 怎麼辦 |
|--|--|--|--|
| HTTP **401** / authentication / 缺 Authorization | `auth` | 1 | **憑證問題**：沒登入或 key 失效 → 填 sk-ant key，或 `llm-login login` |
| **connection refused** | `sidecar` | 3 | **sidecar 沒起**：proxy 沒跑 → `up.sh` 會自動起 |
| 其他 | `other` | 1 | 上游其他錯誤 |

> 關鍵：`401` ⇒ 憑證問題；`connection refused` ⇒ sidecar 沒跑。兩者不同源，別當成「模型沒回應」。
> （C++ binding 編譯期連結，故沒有 janet 版的 `:no-binding` 這一類。）

## Anthropic 帳號登入（OAuth）的現實 ⚠

**「打 Anthropic API」與「OAuth 帳號登入」目前有張力，請先決定要哪條：**

- **Anthropic 直連 API 沒有程式化 OAuth 帳號登入**。它只發 `sk-ant-...` 金鑰、收 `x-api-key`。
  `llm-login` 的 `providers/` **沒有 anthropic**（只有 openrouter / openrouter-deepseek /
  google-gemini / azure-openai / github-models）。所以 `MODE=anthropic` 這條的憑證是**你自己貼
  sk-ant key**，**不是** OAuth 帳號登入。
- **真要 OAuth 帳號登入、又要用到 Claude**：走 **OpenRouter**。`llm-login` 有現成 openrouter preset
  （OAuth PKCE、免註冊 client、換回不過期 key），OpenRouter 一個 OpenAI-compat endpoint 通吃含 Claude，
  **不需 proxy**。`MODE=openrouter ./scripts/up.sh` 就是這條。

要做 OpenRouter OAuth 帳號登入（**需你親自開瀏覽器一次**）：

```sh
cp ~/repo/ai_core/sub_projs/cllm/tools/llm-login/providers/openrouter.json ~/.config/llm/oauth.json
~/repo/ai_core/sub_projs/cllm/build/tools/llm-login login   # 開瀏覽器帳號登入 → 換不過期 key
MODE=openrouter ./scripts/up.sh 你好
```

## 需要你（使用者）親自做的

1. **決定憑證路線**：貼 `sk-ant-...`（直連 Anthropic，非 OAuth）vs OpenRouter OAuth 帳號登入（觸及 Claude）。
2. **真 OAuth 往返要你開瀏覽器**：`llm-login login` 屬 WAIT_USER，無人環境跑不完。
3. **填憑證**：`config/config.json` 的 `api_key`，或 `export ANTHROPIC_API_KEY=...`。
4. **掛好 cllm 環境**：`source ~/dev/cllm/env.sh` 後 `./build.sh` 應能編出 `build/cpp-try-1`。
