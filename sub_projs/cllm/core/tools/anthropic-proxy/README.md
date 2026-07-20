# anthropic-proxy —— 讓 cllm 直連 Anthropic 的本機轉發代理（C++）

← [tools](../README.md)｜[cllm INDEX](../../INDEX.md)｜對照：[llm-login](../llm-login/README.md)

> **補上 [llm-login](../llm-login/README.md) 明說「接不了」的那條路。** cllm 硬編
> `Authorization: Bearer <key>`＋OpenAI wire format（見 [`src/cabi.cpp`](../../src/cabi.cpp)
> `make_request`）；Anthropic 直連 API 收的是 `x-api-key`＋`anthropic-version`＋Messages
> 格式，**兩邊對不上**。這支代理擺中間：cllm 以為在跟 OpenAI 講話，代理當場翻成 Anthropic、
> 回應再翻回去。**cllm 的核心一行不動**——關注點分離。

## 一張圖

```
  llm CLI/lib ──OpenAI＋Bearer──▶  anthropic-proxy  ──Messages＋x-api-key──▶  api.anthropic.com
  (config)     127.0.0.1:8787     (翻請求／翻回應)                          (真上游)
```

**無狀態**：你的 `sk-ant-...` 照樣填進 cllm 的 `config.json`；cllm 把它塞進 `Authorization:
Bearer`，代理當場掏出來轉手成 `x-api-key`——**代理自己不存、不落地任何金鑰**。

## 純 C++、零新依賴

C++23，跟 cllm 同一套建置（CMake＋Ninja＋vcpkg＋glaze）。**入站**用共用微型 server
（[`../common/httpd`](../common/httpd.hpp)，cllm 只有 client）；**出站**重用 cllm 的 http 笨管子
（[`../../src/http.hpp`](../../src/http.hpp)）；翻譯用 glaze `json_t`。除 glaze＋curl（cllm 本就有）
外零新依賴。

> Python 原版保留在 [`reference/`](reference/)（一比一對照、可離線讀）；C++ 版為現行實作。

## 建置

隨 cllm 一起建（`tools/` 已進主建置，可 `-DCLLM_BUILD_TOOLS=OFF` 關）：

```sh
cd sub_projs/cllm
cmake --preset linux-debug
cmake --build --preset linux-debug --target anthropic-proxy   # → build/tools/anthropic-proxy
```

## 用法

**① 起代理**（三選一）：

```sh
./build/tools/anthropic-proxy                    # 直接跑（前景），監聽 127.0.0.1:8787
./tools/anthropic-proxy/service/proxyctl.sh start   # 免 systemd 背景常駐（自動找 build 產物）
# 或 systemd user 服務，見下「常駐背景」
```

**② cllm config 指過來**——最省事複製現成的（挑模型見 [models.json](models.json)）：

```sh
cp tools/anthropic-proxy/configs/opus.json ~/.config/llm/config.json   # 或 sonnet／haiku／fable
$EDITOR ~/.config/llm/config.json                                       # 填 api_key＝你的 sk-ant-...
```

配好長這樣：

```json
{
  "endpoint": "http://127.0.0.1:8787/v1/chat/completions",
  "model":    "claude-opus-4-8",
  "api_key":  "sk-ant-..."
}
```

> ⚠ cllm config 解析**嚴格拒絕未知鍵**（`glz::read_json`），只放 `endpoint`／`model`／`api_key`／
> `max_tokens`／`temperature` 等 cllm 認得的欄，別加 `_notes`（會整份解析失敗）。模型 id 見
> [models.json](models.json)：`claude-opus-4-8`（預設）／`claude-sonnet-5`／`claude-haiku-4-5`／`claude-fable-5`。

**③ 裸跑 cllm**，就打到 Anthropic 了：

```sh
llm 用一句話介紹你自己
llm --stream 寫一首關於轉發代理的短詩          # 串流照樣通（SSE 逐段翻）
```

不想動 config，也可一次性覆蓋：

```sh
llm 哈囉 --endpoint http://127.0.0.1:8787/v1/chat/completions --model claude-opus-4-8 --api-key sk-ant-...
```

## 翻了什麼（覆蓋 cllm 的全部能力）

| cllm 送的（OpenAI） | 代理翻成（Anthropic） |
|--|--|
| `Authorization: Bearer <key>` | `x-api-key: <key>` ＋ `anthropic-version` |
| `messages[].role=="system"` | 抽到頂層 `system`（多條併接） |
| `content` 字串／`text` part | `{type:"text"}` block |
| `image_url`（data URI／http url） | `{type:"image", source:{base64…／url…}}` |
| `tools[{function}]` | `tools[{name, input_schema}]` |
| `response_format(json_schema)` | **強制工具**：逼呼叫 → 回來 input 序列化回 content JSON（還原 schema 語意） |
| assistant `tool_calls`／role=`tool` | `tool_use` 塊／`tool_result` 塊（多輪工具往返） |
| `max_tokens`（選填） | Anthropic 必填 → 沒給墊 `ANTHROPIC_MAX_TOKENS`（預設 4096） |
| `temperature`／`top_p` | 直通 |
| `stream:true` | Messages SSE 逐事件翻成 OpenAI SSE，含 `[DONE]` 收尾 |

**丟棄**（Anthropic 無對應）：`presence_penalty`／`frequency_penalty`／`seed`／`modalities`／音訊輸入。
**上游錯誤**翻成 cllm 認得的 `{"error":{"message":…}}` 並保留 HTTP 狀態碼，讓 cllm 的
`guard_backend_error` 正常攔截（串流模式會先偷看首字元分辨 SSE vs JSON 錯誤體）。

## 常駐背景（不自動啟動——由你決定何時起）

**A. systemd user 服務**（Manjaro 首選，能開機自啟；ExecStart 指 build 產物）：

```sh
mkdir -p ~/.config/systemd/user
ln -sf "$PWD/tools/anthropic-proxy/service/anthropic-proxy.service" ~/.config/systemd/user/
systemctl --user daemon-reload && systemctl --user start anthropic-proxy   # ← 你自己下才會起
journalctl --user -u anthropic-proxy -f
```

**B. 免 systemd 腳本**：`./service/proxyctl.sh {start|stop|status|logs}`（自動找 `build/tools/anthropic-proxy`）。

## 檔案

| 檔 | 是什麼 |
|--|--|
| [translate.{hpp,cpp}](translate.cpp) | 純翻譯層：OpenAI⇄Anthropic 形狀轉換（glaze json_t）。無 I/O、無網路 |
| [proxy.cpp](proxy.cpp) | 執行檔主程式：httpd 入站＋src/http 出站＋chunked 串流。無狀態、金鑰轉手不落地 |
| [models.json](models.json) | 現行 Anthropic 模型清單（資料，非 cllm config） |
| [configs/](configs/) | 現成 cllm config：`cp configs/opus.json ~/.config/llm/config.json` 再填 api_key |
| [service/](service/) | systemd user 單元＋免 systemd 的 proxyctl.sh（都指 build 產物） |
| [reference/](reference/) | Python 原版（一比一對照、已封存為參考，非現行實作） |

## 環境變數

`ANTHROPIC_BASE_URL`（預設 `https://api.anthropic.com`，亦可 `--base`）／`ANTHROPIC_VERSION`
（`2023-06-01`）／`ANTHROPIC_MAX_TOKENS`（`4096`）／`PORT`（`8787`，亦可 `--port`）。

## 驗證

`translate` 的形狀轉換邏輯與 Python 原版一致；端對端 HTTP 管線（非串流回聲、串流組回、`[DONE]`、
缺 key→401、x-api-key/version 帶對）已用假上游對測。真 `api.anthropic.com` 往返需真 sk-ant key
（Claude 跨不過去——見 [WAIT_USER](../../WAIT_USER.md)）。

## 安全與條款

金鑰只在記憶體轉手，代理不寫檔、不記錄 body，預設綁 `127.0.0.1`。這是「直連你自己的 Anthropic
API key」＝正規 API 存取，非消費級訂閱 session。指向哪個 endpoint、是否符合條款，是使用者的責任。
