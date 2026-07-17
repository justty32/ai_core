# llm-login — 用「帳號登入（OAuth）」餵 token 給 cllm

← [tools](../README.md)｜[cllm README](../../README.md)

> **補上 cllm 缺的那一段。** cllm 的 `llm` CLI 不懂「登入」——它只把 config 的 `api_key`
> 塞進 `Authorization: Bearer`（見 [`src/cabi.cpp`](../../src/cabi.cpp) `make_request`）。
> 本工具做 OAuth 2.0 **授權碼＋PKCE** 登入、拿 token、到期自動 refresh，把當前有效
> `access_token` 寫進 cllm 的 `config.json`。**cllm 的 C/C++ 核心一行不動**——關注點分離。

## 這是哪一種「帳號登入」

回應「假如我用帳號登入」的三種意思（詳見 cllm 討論）：

- **① 消費級網頁訂閱**（ChatGPT Plus／Claude Pro 那種月費帳號的 session）——**本工具不做、也別這樣用**。那是網頁 OAuth session，兩家 ToS 都禁止程式化使用消費帳號，token 短命又非 OpenAI-compatible，死路。
- **② 供應商為程式化／API 存取提供的正規 OAuth**——**本工具正是為此**。合法、線路層與 cllm 相容（最終還是 `Authorization: Bearer`）。
- **③ 本機後端**（LM Studio／Ollama）——**根本不用登入**，cllm 預設就指 localhost、`api_key` 留空。真痛點若是「不想申請」，這條最合專案哲學，用不到本工具。

> ⚠ **只對 ② 合法適用。** 本工具供應商中立、不預設任何一家；把它指向哪個 endpoint、是否符合該家條款，是使用者的責任。

## 用法

零外部相依，純 Python 3.11+ 標準庫。先備好 provider 設定：

```sh
cp oauth.example.json ~/.config/llm/oauth.json   # 填成你那家的 authorize_url/token_url/client_id/scopes
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
| [llm_login.py](llm_login.py) | CLI：`login`／`refresh`／`token`／`status` |
| [oauth.example.json](oauth.example.json) | provider 設定範本（複製成 `~/.config/llm/oauth.json` 填值） |
| [test_offline.py](test_offline.py) | 離線煙霧測試（PKCE 正確性／URL 組裝／token store round-trip／config patch），不連網不開瀏覽器 |

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
