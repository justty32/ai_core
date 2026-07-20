# cllm-py — handy 的 LLM 地基（純 Python，可 import／可 pip install）

← [handy](../)（重建中）

這是 **handy 的共用 LLM 地基**：一個零外部相依的純 Python 套件，把 prompt 組成 OpenAI 相容的
`/chat/completions` 請求、打出去、解回應。**未來 handy 的住戶都靠它當「腦」**——不再各自手搓 HTTP、
不再 shell-out 到 C++ `llm` CLI，而是 `from cllm import ask` 直接呼叫。

- **當函式庫**：`from cllm import ask` → `ask("你好")`，`tools`／`media`／`stream`／`schema` 全是 kwargs。
- **當 CLI**：`python -m cllm 你好` 或裝成 `cllm` 指令；旗標鏡像 cllm 的 `llm` CLI。
- **零相依**：只用 Python 標準庫（`urllib`/`json`）；`requires-python >=3.8`。
- **離線可測**：自帶 `test/fixtures/`，`python test/cli_smoke.py` 走 `file://` 假回應，不連網。

> **來源**：搬自 [cllm/core-py](../../cllm/core-py/)（cllm C++ 版的純 Python 平行實作）。那裡是**語意真相源**
> （請求／回應欄位對齊 C++ `core/`）；本副本是 handy 自包含的 vendored 版（連 fixtures 一起帶入，故無
> `../core` 外部相依）。要改請求／回應語意，先動源頭再同步過來。

---

## 給 handy 住戶：怎麼裝、怎麼用

### 1) 本地 pip install（住戶的標準相依方式）

handy 的住戶把**這個本地資料夾**當相依裝進來（editable，改到即生效）：

```sh
# 在住戶自己的環境（或專案 venv）裡：
pip install -e /home/lorkhan/repo/ai_core/sub_projs/handy/cllm-py
# 或用相對路徑（從住戶資料夾往上指）：
pip install -e ../cllm-py
```

裝完就能 `import cllm`，並多出 `cllm`（與 `llm`）兩個指令。
⚠ `llm` 可能與 cllm C++ 版建出的原生 `llm` 撞名——只想避開就用 `cllm`／`python -m cllm`，或用 venv 隔離。

**不裝也能用**：把本資料夾加進 `PYTHONPATH` 即可（`PYTHONPATH=/…/cllm-py python -c "import cllm"`），
CLI 則 `python -m cllm …`（在本資料夾內）。

### 2) 當函式庫 import

```python
from cllm import ask

text = ask("你好")                                      # 只給 prompt（走內建 localhost endpoint）
text = ask("你好", "http://…/chat/completions")         # prompt + endpoint（位置參數）
text = ask("你好", model="deepseek-chat", temperature=0.7)

# 串流：逐段進 on_delta；回呼回真值＝中止（回已收部分）
ask("數到五", stream=True,
    on_delta=lambda piece: print(piece, end="", flush=True))

# tools（function calling）：模型要呼叫工具時觸發 on_tool
ask("東京天氣如何？",
    tools=[{"name": "get_weather", "description": "查天氣",
            "parameters": '{"type":"object","properties":{"city":{"type":"string"}}}'}],
    on_tool=lambda call: print(call["name"], call["arguments"]))   # call={id,name,arguments}
```

`ask()` 回**完整答案字串**；失敗時：給了 `on_error` 就呼叫它並回 `None`，否則 `raise LLMError`。
`tools`／`media`／`modalities` 皆 kwargs、可任意組合。完整簽章與陷阱見 `cllm/core.py` 的模組 docstring。

**endpoint／金鑰**：`ask(..., endpoint=..., api_key=...)`；CLI 走 `--endpoint`／`--api-key` 或 config 檔
（見下）。接 DeepSeek＝endpoint `https://api.deepseek.com/v1/chat/completions` + `DEEPSEEK_API_KEY`。

### 3) 當 CLI

```sh
python -m cllm 用一句話介紹你自己
cat report.txt | python -m cllm 總結這份            # prompt＋stdin 合體
git diff | python -m cllm 把 - 寫成 commit 訊息      # 「-」明指 stdin 插入點
python -m cllm --stream -- --開頭的-prompt           # -- 之後一律當 prompt
python -m cllm --help                                # 完整旗標說明
```

固定旗標：`--system` / `--stream` / `--image`（＝`--media`）/ `--schema` / `--config` /
`--tool` / `--modality` / `--media-out` / `--` / `--help`。
連線／取樣旗標：`--endpoint` / `--api-key` / `--timeout-ms` / `--model` / `--temperature` /
`--top-p` / `--presence-penalty` / `--frequency-penalty` / `--max-tokens` / `--seed`。

設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。config 檔路徑：
`--config <檔>` ＞ 環境變數 `LLM_CLI_CONFIG` ＞ `~/.config/llm/config.json`。

退出碼：`0` 成功；`1` 用法錯；`2` 請求失敗（傳輸／後端／媒體落檔失敗）；`130` 取消。

---

## ⚠ CLI 旗標取值語意（與 cllm C++ `llm` 的刻意分歧）

只發生在本 Python 版：

- **`--schema` / `--tool` / `--modality`：收「字面 JSON 文字」，不讀檔**（同 `--system`）。
  要吃檔案內容一律 `$(cat …)`：
  ```sh
  python -m cllm 給我角色 --schema '{"type":"object","required":["name"]}'
  python -m cllm 給我角色 --schema "$(cat schema.json)"
  python -m cllm 說你好 --modality 'audio={"voice":"alloy"}'   # 名=<字面JSON>；只給 audio＝無參數
  ```
- **`--image`／`--media`（同義）：例外保留讀檔，並按值三分流**——`data:`／`http(s)://` URL 直通不編碼；
  `.json` 結尾當預算好的 media 描述子直通；其餘當二進位圖檔路徑，讀檔+base64（mime 由副檔名推）。

---

## 離線自測

```sh
python test/cli_smoke.py     # 走 test/fixtures/ 的 file:// 假回應，全過回 0（現況 42/42）
```

離線 fixture 驗得到「解析／組裝／退出碼分流」，驗不到「後端錯誤語意」（假回應永遠 200）——那要真後端。
手動指向假回應：`python -m cllm 你好 --endpoint "file://$(pwd)/test/fixtures/fake/chat/completions"`。

## 結構

```
cllm-py/
├── cllm/            # 套件本體（import 名＝cllm）
│   ├── __init__.py  #   re-export ask / LLMError / DEFAULT_ENDPOINT
│   ├── core.py      #   ★ 內核門面：ask 出口＋接線
│   ├── request.py http.py response.py stream.py   # 組請求／傳輸／解回應／SSE 串流
│   ├── cli.py argv.py reqinput.py output.py        # CLI 外殼＋argv 掃描＋組請求＋出口
│   └── errors.py internal.py flags.py config.py media.py
├── test/            # 離線黑箱煙霧測試＋自帶 fixtures/
├── pyproject.toml   # 封裝（name=cllm-core-py、scripts: cllm/llm）
└── README.md
```

每檔單一職責、≤150 行、依賴為無環 DAG（`errors`/`internal` 為葉）。詳細對照與已知落差（SIGINT、
config 未知鍵、四旗標分歧）見 [源頭 core-py 的 README](../../cllm/core-py/README.md)。
