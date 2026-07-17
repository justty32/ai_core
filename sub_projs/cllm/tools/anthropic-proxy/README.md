# anthropic-proxy —— 讓 cllm 打得到 Anthropic 的本機轉發代理

← [tools](../README.md)｜[cllm README](../../README.md)｜對照：[llm-login](../llm-login/README.md)

> **補上 [llm-login](../llm-login/README.md) 明說「接不了」的那條路。** cllm 硬編
> `Authorization: Bearer <key>`＋OpenAI wire format（見 [`src/cabi.cpp`](../../src/cabi.cpp)
> `make_request`）；Anthropic 直連 API 收的是 `x-api-key`＋`anthropic-version`＋Messages
> 格式，**兩邊對不上**。這支代理擺中間：cllm 以為在跟 OpenAI 講話，代理當場翻成
> Anthropic、回應再翻回去。**cllm 的 C/C++ 核心一行不動**——關注點分離。

## 一張圖

```
  llm CLI  ──OpenAI＋Bearer──▶  anthropic-proxy  ──Messages＋x-api-key──▶  api.anthropic.com
 (config)   127.0.0.1:8787       (翻請求／翻回應)                          (真上游)
```

**無狀態**：你的 `sk-ant-...` 照樣填進 cllm 的 `config.json`；cllm 把它塞進 `Authorization:
Bearer`，代理當場掏出來轉手成 `x-api-key`——**代理自己不存、不落地任何金鑰**。

## 為什麼要這個（vs llm-login）

| | [llm-login](../llm-login/README.md) | **anthropic-proxy** |
|--|--|--|
| 解的問題 | 沒 key → 用 OAuth 帳號登入換 token | 有 key ，但 **wire format／認證頭對不上** |
| 手法 | 把 token patch 進 config 的 `api_key` | 翻譯 HTTP 請求／回應形狀 |
| 對 Anthropic 直連 | **不適用**（Anthropic 無 API-OAuth） | **正是為此**（x-api-key＋Messages 翻譯）|

llm-login 給的是「透過 OpenRouter 借道摸 Claude」；本工具給的是「**拿你自己的 Anthropic
key 直連 `api.anthropic.com`**」。兩條路都通，挑一條。

## 用法

零外部相依，純 Python 3.11+ 標準庫（`http.server`＋`urllib`）。

**① 起代理**（放前景或背景／`nohup`／tmux 皆可）：

```sh
python3 proxy.py                         # 監聽 127.0.0.1:8787 → https://api.anthropic.com
python3 proxy.py --port 9000             # 換埠
python3 proxy.py --base http://localhost:8080   # 換上游（自架 Anthropic-compat 閘道時）
```

**② 把 cllm config 指過來**——最省事是複製一份現成的（見 [models.json](models.json) 挑模型）：

```sh
cp configs/opus.json ~/.config/llm/config.json      # 或 sonnet／haiku／fable
$EDITOR ~/.config/llm/config.json                    # 把 api_key 換成你的 sk-ant-...
```

配好長這樣（`~/.config/llm/config.json`，或用 `--endpoint`/`--model`/`--api-key` 旗標當場覆蓋）：

```json
{
  "endpoint": "http://127.0.0.1:8787/v1/chat/completions",
  "model":    "claude-opus-4-8",
  "api_key":  "sk-ant-..."
}
```

> ⚠ cllm 的 config 解析**嚴格拒絕未知鍵**（`glz::read_json`），只放 `endpoint`／`model`／`api_key`／
> `max_tokens`／`temperature` 等 cllm 認得的欄，別加 `_notes` 之類（會整份解析失敗）。
> 現行 Anthropic 模型 id 見 [models.json](models.json)：`claude-opus-4-8`（預設）／`claude-sonnet-5`／
> `claude-haiku-4-5`／`claude-fable-5`。

**③ 裸跑 cllm**，就打到 Anthropic 了：

```sh
llm 用一句話介紹你自己
llm --stream 寫一首關於轉發代理的短詩          # 串流照樣通（SSE 逐段翻）
```

不想動 config，也可一次性覆蓋：

```sh
llm 哈囉 --endpoint http://127.0.0.1:8787/v1/chat/completions --model claude-sonnet-5 --api-key sk-ant-...
```

## 翻了什麼（覆蓋 cllm 的全部能力）

| cllm 送的（OpenAI） | 代理翻成（Anthropic） |
|--|--|
| `Authorization: Bearer <key>` | `x-api-key: <key>` ＋ `anthropic-version` |
| `messages[].role=="system"` | 抽到頂層 `system`（多條併接） |
| `content` 字串／`text` part | `{type:"text"}` block |
| `image_url`（data URI／http url） | `{type:"image", source:{base64…／url…}}` |
| `tools[{function}]` | `tools[{name, input_schema}]` |
| `response_format(json_schema)` | **強制工具**：逼呼叫 → 回來時 input 序列化回 content JSON（還原 schema 語意） |
| assistant `tool_calls`／role=`tool` | `tool_use` 塊／`tool_result` 塊（多輪工具往返） |
| `max_tokens`（選填） | Anthropic 必填 → 沒給墊 `ANTHROPIC_MAX_TOKENS`（預設 4096） |
| `temperature`／`top_p` | 直通 |
| `stream:true` | Messages SSE 逐事件翻成 OpenAI SSE，含 `[DONE]` 收尾 |

**丟棄**（Anthropic Messages 無對應）：`presence_penalty`／`frequency_penalty`／`seed`／
`modalities`／音訊輸入——靜默略過，不影響其餘。**上游錯誤**（4xx/5xx）翻成 cllm 認得的
`{"error":{"message":…}}` 並保留 HTTP 狀態碼，讓 cllm 的 `guard_backend_error` 正常攔截。

## 常駐背景（不自動啟動——由你決定何時起）

**A. systemd user 服務**（Manjaro 首選，能開機自啟）：

```sh
mkdir -p ~/.config/systemd/user
ln -sf "$PWD/service/anthropic-proxy.service" ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user start anthropic-proxy            # ← 你自己下這行才會起
systemctl --user status anthropic-proxy
journalctl --user -u anthropic-proxy -f           # 看日誌
# 開機自啟（可選）：systemctl --user enable anthropic-proxy
#   要無登入也常駐：loginctl enable-linger $USER
```

單元檔已收緊沙箱（`ProtectSystem=strict`／`ProtectHome=read-only`／`NoNewPrivileges`）——代理純
stdlib、只需 loopback 出網，且**不讀環境金鑰**（金鑰仍由 cllm 送、代理轉手）。

**B. 免 systemd 一鍵腳本**（或不想動 systemd 時）：

```sh
./service/proxyctl.sh start      # nohup 背景起，PID／日誌落在 XDG runtime 目錄
./service/proxyctl.sh status
./service/proxyctl.sh logs       # tail -f
./service/proxyctl.sh stop
```

> 兩種都**不會被別的東西自動叫起**——要跑代理，你自己下 `start`。

## 檔案

| 檔 | 是什麼 |
|--|--|
| [translate.py](translate.py) | 純翻譯層：OpenAI⇄Anthropic 的請求／回應／串流形狀轉換。無 I/O、無網路、可離線測 |
| [proxy.py](proxy.py) | 伺服器：`http.server` 收 OpenAI 請求、`urllib` 轉發 Anthropic、chunked 回串流。無狀態、金鑰轉手不落地 |
| [test_offline.py](test_offline.py) | 離線煙霧測試（請求／回應／錯誤／串流四路的形狀），不連網不起伺服器 |
| [models.json](models.json) | 現行 Anthropic 模型清單（資料，非 cllm config）：id／context／價格／挑哪個 |
| [configs/](configs/) | 現成 cllm config：`cp configs/opus.json ~/.config/llm/config.json` 再填 api_key。只含 cllm 認得的鍵 |
| [service/anthropic-proxy.service](service/anthropic-proxy.service) | systemd user 單元（背景常駐、可開機自啟、已收緊沙箱）|
| [service/proxyctl.sh](service/proxyctl.sh) | 免 systemd 的 start／stop／status／logs 一鍵腳本 |

## 環境變數

| 變數 | 預設 | 作用 |
|--|--|--|
| `ANTHROPIC_BASE_URL` | `https://api.anthropic.com` | 上游 base（亦可 `--base`）|
| `ANTHROPIC_VERSION` | `2023-06-01` | `anthropic-version` 頭 |
| `ANTHROPIC_MAX_TOKENS` | `4096` | cllm 沒送 `max_tokens` 時的墊值 |
| `PORT` | `8787` | 監聽埠（亦可 `--port`）|

## 驗證

```sh
python3 test_offline.py       # 全離線，應全綠（翻譯層 30 條斷言）
```

真往返要**真 Anthropic key**，Claude 跨不過去（同 [llm-login](../llm-login/README.md) 的
[WAIT_USER](../../WAIT_USER.md) 情形）。端對端 HTTP 管線（含 chunked 串流、缺 key→401、
x-api-key/version 帶對）已用假上游對測過。

## 安全與條款

- 金鑰**只在記憶體轉手**，代理不寫檔、不記錄 body。監聽預設綁 `127.0.0.1`，別無腦開到 `0.0.0.0`。
- 這是「直連你自己的 Anthropic API key」，走的是**正規 API 存取**——不是消費級訂閱 session
  （那條路 llm-login 明說不做）。指向哪個 endpoint、是否符合條款，仍是使用者的責任。
