# llm-login — 用「帳號登入（OAuth）」餵 token 給 cllm

← [tools](../README.md)｜[cllm README](../../README.md)

> **補上 cllm 缺的那一段。** cllm 的 `llm` CLI 不懂「登入」——它只把 config 的 `api_key`
> 塞進 `Authorization: Bearer`（見 [`src/cabi.cpp`](../../src/cabi.cpp) `make_request`）。
> 本工具做 OAuth 2.0 **授權碼＋PKCE** 登入、拿 token、到期自動 refresh，把當前有效
> `access_token` 寫進 cllm 的 `config.json`。**cllm 的 C/C++ 核心一行不動**——關注點分離。

## 主流供應商：能接的與接不了的

「主流的都要」——但**主流 LLM 供應商在「OAuth 帳號登入換 API 存取」這件事上分兩派**，不是每家都能接：

**能接（真・程式化 OAuth，`providers/` 有現成 preset）**——全部是 OpenAI-compatible endpoint＋Bearer 認證：

| Preset | 流程 | 你要先備 | cllm endpoint |
|--------|------|----------|---------------|
| [openrouter](providers/openrouter.json) | PKCE，**免註冊 client** | 一個 OpenRouter 帳號 | 單一 endpoint 通吃全模型 |
| [google-gemini](providers/google-gemini.json) | 標準 OAuth | Google Cloud OAuth Client ID＋scope | Gemini OpenAI-compat |
| [azure-openai](providers/azure-openai.json) | 標準 OAuth（Entra ID） | Entra app＋tenant＋資源角色 | 你的 Azure 資源/deployment |
| [github-models](providers/github-models.json) | 標準 OAuth | GitHub OAuth App | GitHub Models（免費額度）|

**接不了（key-only，無正規程式化 OAuth，本工具不適用）**：

- **DeepSeek**：只有 `sk-` API key，官方**沒有** OAuth-換-API（唯一的 OAuth 是用 Google/Apple 登入 DeepSeek 網站本身，不換 API token）。**但它是 OpenAI-compat＋Bearer**，所以不需要本工具——直接把 key 填進 cllm config 就能跑：`{"endpoint":"https://api.deepseek.com/chat/completions","model":"deepseek-chat","api_key":"sk-..."}`。想「用 OAuth 登入」摸到 DeepSeek，走 **OpenRouter preset**、把 `cllm_model` 設成 `deepseek/deepseek-chat` 即可。
- **OpenAI 直連 API**：只有 `sk-` API key，官方**沒有** OAuth-換-API。網路上「用 ChatGPT 帳號」的做法＝消費訂閱 session＝下面的 ①，不做。（同樣 OpenAI-compat＋Bearer，直接填 key 即可。）
- **Anthropic 直連 API**：只有 `x-api-key`（**連 `Authorization: Bearer` 都不是**、非 OpenAI wire format），cllm 現況本就接不上；存在的 OAuth 是 Claude Code 訂閱登入（scoped client）＝① 訂閱路，不做。
- 一句話：這些家要嘛**直接填 API key** 進 config（DeepSeek/OpenAI 因是 OpenAI-compat 最順），要嘛透過 **OpenRouter OAuth** 借道，要嘛走**本機後端**。

> ⚠ **最省事推薦 OpenRouter**：免註冊 OAuth client、登入完拿一把不過期的 user API key、一個 endpoint 通吃——最貼近「不想逐家申請 key」的原始痛點。

### 三種「帳號登入」的定位（回顧）

- **① 消費級網頁訂閱**（ChatGPT Plus／Claude Pro 的 session）——**不做、也別這樣用**：ToS 禁止程式化使用消費帳號，token 短命又非 OpenAI-compatible。
- **② 供應商為 API 存取提供的正規 OAuth**——**本工具正是為此**，上表四家即是。
- **③ 本機後端**（LM Studio／Ollama）——**根本不用登入**，cllm 預設指 localhost、`api_key` 留空。

> 本工具供應商中立；指向哪個 endpoint、是否符合該家條款，是使用者的責任。

## 用法

零外部相依，純 Python 3.11+ 標準庫。先備好 provider 設定——**挑一個 preset 複製、填 `_notes` 交代的空**（`<...>` 佔位）：

```sh
cp providers/openrouter.json ~/.config/llm/oauth.json     # 最省事；OpenRouter 幾乎不用改
# 或指定別家：cp providers/google-gemini.json ~/.config/llm/oauth.json（填 client_id/secret/scope）
# 也可不複製、用環境變數指到 preset：export LLM_OAUTH_PROVIDER=$PWD/providers/openrouter.json
# 供應商中立範本（自己填全部）：oauth.example.json
```

```sh
python3 llm_login.py login      # 開瀏覽器登入一次 → 拿 token、寫進 cllm config.json
python3 llm_login.py status     # 看目前狀態（不外洩 token 本身）
python3 llm_login.py refresh    # 手動用 refresh_token 換新 access_token
python3 llm_login.py token      # 印當前有效 token（快過期自動 refresh）
```

登入後，裸跑 cllm 就帶著 bearer 了（因為 `config.json` 的 `api_key` 已被寫入）：

```sh
llm 用一句話介紹你自己 --endpoint https://你的/v1/chat/completions --model 你的model
```

短命 token 要嘛靠 `token` 子命令即時取（腳本場景）：

```sh
llm 你好 --api-key "$(python3 llm_login.py token)"   # token 過期會自動 refresh 再印
```

## 檔案

| 檔 | 是什麼 |
|----|--------|
| [oauth.py](oauth.py) | OAuth 協定層：PKCE、組 authorize URL、本機 callback 接碼、換 token／refresh。供應商中立、不硬編任何一家 |
| [store.py](store.py) | 狀態層：讀 provider 設定、token 存放（0600）、回頭只 patch cllm `config.json` 的 `api_key`（不碰其他鍵、不塞 glaze 不認得的鍵） |
| [llm_login.py](llm_login.py) | CLI：`login`／`refresh`／`token`／`status`（依 `flow` 分標準 OAuth 與 OpenRouter 兩路）|
| [providers/](providers/) | **主流供應商現成 preset**（endpoint 皆已上網核對）：openrouter／google-gemini／azure-openai／github-models。每檔 `_notes` 交代要填什麼 |
| [oauth.example.json](oauth.example.json) | 供應商中立範本（要接上表以外的家、自己填全部時用） |
| [test_offline.py](test_offline.py) | 離線煙霧測試（PKCE／標準＋OpenRouter URL 組裝／token store round-trip／config patch／四 preset 可載），不連網不開瀏覽器 |

## 三個檔的落點（都在 `~/.config/llm/`，可用環境變數改）

| 檔 | 誰寫 | 內容 |
|----|------|------|
| `oauth.json` | 使用者手填（或 `LLM_OAUTH_PROVIDER` 指定） | provider 設定 |
| `oauth_token.json` | 本工具（0600） | access／refresh／expires_at |
| `config.json` | cllm CLI 的設定；本工具**只 patch `api_key`**（或 `LLM_CLI_CONFIG` 指定） | 其餘鍵原樣保留 |

## 驗證

```sh
python3 test_offline.py       # 全離線，應全綠
```

真 OAuth 往返（`login`/`refresh` 打真 endpoint＋開瀏覽器）本質要**真供應商＋真帳號**，
Claude 跨不過去——見 [WAIT_USER](../../WAIT_USER.md)。
