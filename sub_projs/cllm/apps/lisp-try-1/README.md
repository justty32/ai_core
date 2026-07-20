# lisp-try-1 —— 用 Common Lisp 打 Anthropic API 的最小 cllm 應用

一支用 **Common Lisp（SBCL）** 寫、透過 **cllm** 打 **Anthropic API** 的最小應用，整合 cllm 的兩支
周邊工具（**anthropic-proxy** 轉發代理 + **llm-login** 帳號登入）。與姊妹專案 `janet-try-1`（Janet 版）
同構，只是換語言。照本機 Common Lisp 慣例範本 `~/code/cl-lab`（ASDF + `src/` + clingon + `scripts/run.sh`）搭建。

## 架構

```
  lisp app  ──(load CLLM_LISP)──▶  cllm CL binding (libcllm.so)  ──OpenAI+Bearer──▶  anthropic-proxy
  src/main.lisp                    (cllm:ask ...)                   127.0.0.1:8787    (sidecar，翻 wire format)
                                                                                             │ x-api-key
                                                                                             ▼
                                                                                    api.anthropic.com
  憑證來源：貼 sk-ant key（MODE=anthropic）或 llm-login OAuth 帳號登入（MODE=openrouter，觸及 Claude）
```

- **cllm** 對外只認「`endpoint` + `Bearer api_key`」。Anthropic 收的是 `x-api-key` + Messages 格式，
  兩邊對不上，所以 **anthropic-proxy** 坐在 endpoint 上當本機 sidecar 翻譯，**cllm 核心一行不動**。
- app 本身不碰這些細節——它只在執行期 `(load CLLM_LISP)` 取得 `cllm:ask`，endpoint 指向本機 proxy。
- cllm 的 CL binding **已裝好**（`~/dev/lib/lisp/cllm.lisp`，由 `CLLM_LISP` 環境變數定位；libcllm.so
  由 `LIBCLLM` 定位）。本專案不自帶 binding，靠 `~/dev/cllm/env.sh` 掛上。

## 結構

```
lisp-try-1.asd             ASDF 系統定義（依賴 clingon；cllm binding 靠 CLLM_LISP 執行期 load）
src/package.lisp           套件（命名空間）
src/app.lisp               核心：binding 解析（resolve-ask）+ 失敗分流（classify-error）+ ask 包裝
src/main.lisp              clingon CLI 進入點：讀 endpoint/key/model → (ask) → 印 / 分流
tests/main.lisp            fiveam 離線測（不觸網、不需 binding）：驗失敗分流
config/config.example.json cllm config 範本（proxy 路徑；填 api_key＝你的 sk-ant-...）
scripts/run.sh             dev 小工具：run / test / build / repl（照 cl-lab 慣例）
scripts/up.sh              一鍵佈線：起 proxy sidecar + 備憑證 + 跑 app（兩種 MODE）
scripts/vendor.sh          把 cllm tools 的 build 產物 vendored 進 vendor/cllm-tools/
vendor/cllm-tools/         vendored 二進位（vendor.sh 產出，gitignored）
```

## 怎麼跑

### 0. 前置（一次）

```sh
# a. 建好 cllm + 兩支 tool（若尚未；產物通常已存在，存在就別重建）
cd ~/repo/ai_core/sub_projs/cllm
cmake --preset linux-debug && cmake --build --preset linux-debug
#    產物：build/tools/{anthropic-proxy, llm-login, liblogin.so}

# b. 掛上 cllm 開發環境（含 CLLM_LISP / LIBCLLM；run.sh、up.sh 會自動 source，這步可省）
source ~/dev/cllm/env.sh

# c.（可選）把 tool 產物 vendored 進 app，之後與 repo 解耦
cd ~/code/cllm-apps/lisp-try-1 && ./scripts/vendor.sh
```

> clingon / fiveam 由 quicklisp（`~/quicklisp`）提供，`run.sh` 會自動 quickload，無需另裝。

### 1. 離線自檢（不觸網）

```sh
scripts/run.sh run --check     # 看 binding 就緒？endpoint/key 齊？失敗分流對不對
scripts/run.sh test            # 離線單元測試（fiveam）
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

`up.sh` 做三件事：① 確保 proxy sidecar 在跑（TCP health-check）② 備好憑證 ③ 帶
`APP_ENDPOINT/APP_API_KEY/APP_MODEL` 進來跑 lisp app。

### 3. 編獨立執行檔（可選）

```sh
scripts/run.sh build           # → build/lisp-try-1（自帶 SBCL runtime，執行期仍靠 CLLM_LISP/LIBCLLM 定位 binding）
```

## 失敗怎麼分辨（照 tools/INTEGRATION.md）

app 把 cllm 的錯誤分四類、退出碼帶語意：

| 症狀 | 類別 | 退出碼 | 意思 / 怎麼辦 |
|--|--|--|--|
| HTTP **401** / authentication / 缺 Authorization | `:auth` | 1 | **憑證問題**：沒登入或 key 失效 → 填 sk-ant key，或 `llm-login login` |
| **connection refused** | `:sidecar` | 3 | **sidecar 沒起**：proxy 沒跑 → `up.sh` 會自動起 |
| `(load CLLM_LISP)` 失敗 / 找不到 | `:no-binding` | 4 | binding 尚未就緒 → `source ~/dev/cllm/env.sh` 設好 CLLM_LISP/LIBCLLM |
| 其他 | `:other` | 1 | 上游其他錯誤 |

> 關鍵：`401` ⇒ 憑證問題；`connection refused` ⇒ sidecar 沒跑。兩者不同源，別當成「模型沒回應」。
> 已離線驗證：**無 proxy → :sidecar(3)**；**有 proxy、無 key → :auth(1)**（proxy 起了、只是憑證缺）。

## Anthropic 帳號登入（OAuth）的現實 ⚠

**「打 Anthropic API」與「OAuth 帳號登入」是兩條路，本專案兩條都留：**

- **`MODE=anthropic`（預設）—— 直連 Anthropic，非 OAuth。**
  Anthropic 直連 API **沒有程式化 OAuth 帳號登入**，它只發 `sk-ant-...` 金鑰、收 `x-api-key`。
  `llm-login` 的 `providers/` **沒有 anthropic**（只有 openrouter / google-gemini / azure-openai /
  github-models）。所以這條的憑證是**你自己貼 sk-ant key**，經 anthropic-proxy 翻成 `x-api-key`
  直連 `api.anthropic.com`。這裡的「登入」＝填 key，**不是** OAuth 帳號登入。

- **`MODE=openrouter` —— 真・OAuth 帳號登入，且觸及 Claude。**
  `llm-login` 有現成 openrouter preset（OAuth PKCE、免註冊 client、換回不過期 key），OpenRouter
  一個 OpenAI-compat endpoint 通吃含 Claude，**不需 proxy**。

要做 OpenRouter OAuth 帳號登入（**需你親自開瀏覽器一次，屬 WAIT_USER**）：

```sh
cp ~/repo/ai_core/sub_projs/cllm/core/tools/llm-login/providers/openrouter.json ~/.config/llm/oauth.json
llm-login login        # 開瀏覽器帳號登入 → 換不過期 key
MODE=openrouter ./scripts/up.sh 你好
```

> 不捏造 anthropic OAuth 端點——providers 裡確實沒有 anthropic，維持現況。

## 需要你（使用者）親自做的

1. **決定憑證路線**：貼 `sk-ant-...`（`MODE=anthropic`，直連 Anthropic、非 OAuth）
   vs OpenRouter OAuth 帳號登入（`MODE=openrouter`，觸及 Claude）。
2. **真 OAuth 往返要你開瀏覽器**：`llm-login login` 屬 WAIT_USER，無人環境跑不完。
3. **填憑證**：`config/config.json` 的 `api_key`，或 `export ANTHROPIC_API_KEY=...`，
   或（OpenRouter）跑一次 `llm-login login`。
4. **確認 CL binding 就緒**：`source ~/dev/cllm/env.sh` 後 `scripts/run.sh run --check`
   應顯示「cllm CL binding：就緒」。
