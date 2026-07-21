# handy — 把 OS 當 AI agent 的試驗田（rebuild on litellm）

handy＝**路徑一（把作業系統當 AI agent）的落地試驗田**：一組小程式／小腳本，拿現成件用薄殼包裝、
按慣例組合。北極星＝整個 OS 變成一個 agent（工具集，非單體）。

本輪**淨空重來**，LLM 地基＝[`util.llm`](util/llm/)（litellm 的薄包裝）。自寫的純 Python 地基
`pllm` 與更早的實作全部凍結於 [`archive/`](archive/)（歷史保留、可還原；要看以前的住戶／設計脈絡
進那裡）。

## 結構

```
handy/
├─ util/      通用共用 lib（namespace 套件）：很多 .py 共用的小工具
│  └─ llm/    LLM 地基：litellm 包裝，CLI 形狀鏡像已封存的 pllm。★住戶的「腦」
├─ llme.py    住戶①：多 endpoint dispatcher（單檔）。換 endpoint 名＝換模型
├─ llme.json  llme 的模型設定（多個模型定義在一檔）
└─ archive/   舊實作凍結（pllm、Fennel 版 llme/zhtw/wf/mail、hermy…）
```

- **每個住戶**：單檔 `.py` 或資料夾（folder-as-callable）。**入口＝檔頭 docstring**（小住戶不另開 README）。
- **地基與 lib**：`util.llm`＝LLM 呼叫；`util` 其餘＝跨住戶共用的純工具。住戶靠它們，別各自重造。

> ⚠ **零相依鐵律的刻意例外**：`util.llm` 依賴 **litellm**（外部套件）。handy 其餘部分維持零相依；
> 這條例外是使用者決定「不自己造 LLM client」的結果，取捨脈絡見 [SESSION-LOG](SESSION-LOG.md)。

---

## util — 通用共用 lib

**定位**：`handy/util/` 是「很多 .py 共用的小工具庫」——純函式、無副作用、不綁特定住戶。
它是 **Python 3.3+ 隱式命名空間套件**：**不需要 `__init__.py`**，往裡丟 `.py` 就多一個子模組
（`util/config.py` → `util.config`、日後 `util/http.py` → `util.http`…）。

**現有模組**：

| 模組 | 用途 |
|---|---|
| `util.config` | 讀 JSON ＋ 解析 `$env`／`$ref`（見下）|
| `util.env` | 環境變數小工具：`first(*keys)`（第一個非空值）、`stem(name)`（名字→`ENV_IDENT`）|
| `util.llm` | **LLM 地基**（子套件，見下節）：litellm 包裝，轉出 `ask`／`cli_main`／`LLMError` |

**住戶怎麼吃**（推薦：放上 `sys.path` 正常 import）：

```python
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)          # 住戶在 handy/ 根：HERE 即 handy/，util/ 是其子夾
from util import config            # 之後 config.read(...) / config.resolve(...)
```

> 住戶若在子資料夾（folder-as-callable），把 `sys.path` 指到 **handy 根**（util 的父層）即可。
> 另有 `importlib.util.spec_from_file_location(...)` 能「按路徑載入單檔、不放 path」——但樣板多、
> 內部互 import 麻煩，**只在載入非固定位置的單檔時才划算**；共用 lib 走上面的 namespace 套件路。

### util.config — 讀 JSON ＋ 解析 `$env` / `$ref`

```python
from util import config
cfg = config.read("some.json")     # 回「完全解析後」的 dict
```

設定檔裡任意欄位值可放**指令式節點**，`read()` 就地替換：

| 節點 | 意思 |
|---|---|
| `{"$env": "VAR"}` | 取環境變數 `VAR`；未設（或空字串）→ `None`（該欄位視為缺） |
| `{"$env": "VAR", "default": "…"}` | 同上，未設時用 `default` |
| `{"$ref": "a.b.c"}` | 引用本檔其他值（從根算 dotted 路徑）；遞迴解析、含 `$ref` 迴圈防護 |

**用意**：secret（金鑰）用 `$env` 從環境帶、不寫死進版控；重複值（共用端點等）用 `$ref` 收斂成一處。

---

## util.llm — LLM 地基（住戶的「腦」）

litellm 的薄包裝，**對外形狀＝已封存的 pllm**：`ask()` 簽章與整套 CLI 旗標逐項照舊，換腦不換介面。

```python
from util.llm import ask          # 函式庫用法
text = ask("你好")                                    # 走內建 localhost endpoint
text = ask("你好", "https://…/v1/chat/completions", model="deepseek-chat")
ask("數到五", stream=True, on_delta=lambda s: print(s, end=""))
```

```python
from util.llm import cli_main     # 借用整套 CLI（llme 就是這樣透傳）
```

**檔案**（每檔單一職責、≤150 行，依賴為無環 DAG，`errors` 為葉）：

| 檔 | 職責 |
|---|---|
| `core.py` | `ask()` 門面＋回呼接線 |
| `call.py` `msg.py` | **去程**：參數→litellm kwargs／組 messages |
| `resp.py` | **回程**：litellm 回應→四個回呼（fixture 也走這條） |
| `cli.py` `argv.py` `flags.py` `cfg.py` `req.py` `media.py` `out.py` | CLI 外殼（沿用 pllm 的分檔） |
| `test/smoke.py` | 離線冒煙測試 |

### ⚠ 與已封存 pllm 的刻意分歧（litellm 帶來的）

- **`--endpoint` 仍收完整 URL**（`…/v1/chat/completions`）＝舊介面不動；內部剝成 litellm 的
  `api_base`（`…/v1`）。
- **model 自動加 `openai/` 前綴**走 OpenAI-compatible 路徑，否則 litellm 會把 `google/gemma-…` 的
  `google/` 誤判成 provider。`LLM_RAW_MODEL=1` 可關掉（要打原生 provider 時用）。
- **`drop_params` 開著**：後端不吃的取樣參數自動丟棄而非炸掉。
- **沒給 `--api-key` 時會補一個佔位字串**。舊 pllm 沒 key 就不送 `Authorization` header；litellm 的
  openai provider 卻**一律**要求 key，不給就當場丟 `InternalServerError`——連本機 LM Studio 這種免認證
  端點都打不出去（`llme local` 首當其衝）。佔位值免認證後端會忽略。
- **`file://` 端點＝離線 fixture 逃生口**：不經 litellm、直接讀該檔當一份**非串流**回應（串流的
  SSE fixture 不支援）。只為無後端自測。

`--schema`／`--tool`／`--modality` 收**字面 JSON 文字不讀檔**（同 `--system`，長內容用 `$(cat s.json)`）；
`--image`／`--media` 例外保留讀檔並三分流。退出碼：`0` 成功／`1` 用法錯／`2` 請求失敗／`130` 取消。
完整旗標見 `python -c "from util.llm import cli_main; cli_main(['llm','--help'])"` 或 `./llme.py <ep> --help`。

### 相依與自測

```sh
pip install litellm                  # ⚠ 唯一外部相依；沒裝時打真端點會報「litellm 未安裝」
python util/llm/test/smoke.py        # 離線冒煙（不連網、不需 litellm）；現況 30/30
```

**本機（Windows 開發機）已備好 venv**：`.venv/`（官方簽名 python 3.13＋litellm 1.91.4，版控忽略）。
用它跑＝`./.venv/Scripts/python.exe llme.py …`。為何非用官方簽名版不可、為何釘 1.91.x，見
[SESSION-LOG](SESSION-LOG.md) 的「開發環境」節。

---

## llme — 多 endpoint dispatcher（住戶①）

**換 endpoint 名＝換模型**：`llme <endpoint> [util.llm 的其餘參數...]`。從 `llme.json` 挑一組設定、
翻成連線旗標，其餘參數原樣透傳，再**重用 `util.llm` 自己的 CLI 解析**跑一次（不 subprocess）。

```sh
./llme.py deepseek 一句話介紹你自己      # 雲端 DeepSeek（需 DEEPSEEK_API_KEY）
./llme.py deepseek --stream 你好         # 透傳 --stream 等所有 util.llm 旗標
./llme.py local 你好                     # 本機 LM Studio（無 key；目前多離線）
./llme.py --help                         # 用法＋可用 endpoint
```

上 PATH 當裸命令：`ln -sf "$(pwd)/llme.py" ~/.local/bin/llme`（有 shebang，直接可執行）。

### llme.json（多模型定義在一檔）

`<endpoint 名> → { endpoint, model, timeout_ms, api_key? }`。用 `util.config` 讀，故吃 `$env`/`$ref`：
金鑰寫成 `{"$env":"DEEPSEEK_API_KEY"}`、共用端點用 `{"$ref":"_lmstudio"}`；`_` 開頭鍵非 endpoint
（當共用資料）。**加模型＝在此加一列**。

### api_key 來源（cascade，前者優先）

1. 使用者顯式 `--api-key`（在透傳參數裡）→ 尊重、不注入
2. `llme.json` 該 endpoint 的 `api_key`（通常 `{"$env":"…"}`）
3. env auto-inject：`LLME_KEY_<EP>` ＞ `<EP>_API_KEY`（`<EP>`＝endpoint 名大寫、非英數轉 `_`）

本地免 key 的 endpoint（2、3 都沒有）不受影響。

### 環境變數

| 變數 | 作用 |
|--|--|
| `LLME_CONFIG` | 覆寫設定檔路徑（預設＝同層 `llme.json`）|
| `LLME_DRY=1` | 不真打，改印「會傳給 `util.llm` 的 argv」到 stderr 後 exit 0（冒煙自測）|
| `LLME_KEY_<EP>` / `<EP>_API_KEY` | auto-inject 的 api key 來源（見 cascade ③）|

### 冒煙自測（不需真後端）

```sh
LLME_DRY=1 ./llme.py local --stream 你好                          # local 無 key，不注入
DEEPSEEK_API_KEY=FAKE LLME_DRY=1 ./llme.py deepseek hi             # $env 帶入 → …--api-key FAKE hi
DEEPSEEK_API_KEY=FAKE LLME_DRY=1 ./llme.py deepseek --api-key mine hi  # 使用者自帶 → 不注入
./llme.py nonexist                                                # 找不到 → 列可用 endpoint，exit 2
# 端到端（走真 util.llm，用 fixture 假回應）：
./llme.py deepseek 你好 --endpoint "file://$(pwd)/util/llm/test/fixtures/text.json"
```
