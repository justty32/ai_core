# handy — 把 OS 當 AI agent 的試驗田（rebuild on pllm）

handy＝**路徑一（把作業系統當 AI agent）的落地試驗田**：一組小程式／小腳本，拿現成件用薄殼包裝、
按慣例組合。北極星＝整個 OS 變成一個 agent（工具集，非單體）。

本輪**淨空重來**，以純 Python 的 LLM 地基 [`pllm`](pllm/) 重構。舊實作全部凍結於 [`archive/`](archive/)
（歷史保留、可還原；要看以前的住戶／設計脈絡進那裡）。

## 結構

```
handy/
├─ pllm/      LLM 地基：cllm 的純 Python 版（零相依）。可 import／pip install。★住戶的「腦」
├─ util/      通用共用 lib（namespace 套件）：很多 .py 共用的小工具。目前 config.py（讀 JSON＋$env/$ref）
├─ llme.py    住戶①：多 endpoint dispatcher（單檔）。換 endpoint 名＝換模型
├─ llme.json  llme 的模型設定（多個模型定義在一檔）
└─ archive/   舊實作凍結（Fennel 版 llme/zhtw/wf/mail、hermy…）
```

- **每個住戶**：單檔 `.py` 或資料夾（folder-as-callable）。**入口＝檔頭 docstring**（小住戶不另開 README）。
- **地基與 lib**：`pllm`＝LLM 呼叫；`util`＝跨住戶共用的純工具。住戶靠它們，別各自重造。

---

## pllm — LLM 地基（住戶的「腦」）

cllm 的純 Python 版，`from pllm import ask` 直接呼叫（`tools`/`media`/`stream`/`schema` 皆 kwargs），
或當 CLI `python -m pllm`。完整用法、pip install、與 C++ `llm` 的旗標分歧見 **[pllm/README.md](pllm/README.md)**。

---

## util — 通用共用 lib

**定位**：`handy/util/` 是「很多 .py 共用的小工具庫」——純函式、無副作用、不綁特定住戶。
它是 **Python 3.3+ 隱式命名空間套件**：**不需要 `__init__.py`**，往裡丟 `.py` 就多一個子模組
（`util/config.py` → `util.config`、日後 `util/http.py` → `util.http`…）。

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

## llme — 多 endpoint dispatcher（住戶①）

**換 endpoint 名＝換模型**：`llme <endpoint> [pllm 的其餘參數...]`。從 `llme.json` 挑一組設定、
翻成 pllm 連線旗標，其餘參數原樣透傳，再**重用 pllm 自己的 CLI 解析**跑一次（不 subprocess）。

```sh
./llme.py deepseek 一句話介紹你自己      # 雲端 DeepSeek（需 DEEPSEEK_API_KEY）
./llme.py deepseek --stream 你好         # 透傳 --stream 等所有 pllm 旗標
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
| `LLME_DRY=1` | 不真打，改印「會傳給 pllm 的 argv」到 stderr 後 exit 0（冒煙自測）|
| `LLME_KEY_<EP>` / `<EP>_API_KEY` | auto-inject 的 api key 來源（見 cascade ③）|

### 冒煙自測（不需真後端）

```sh
LLME_DRY=1 ./llme.py local --stream 你好                          # local 無 key，不注入
DEEPSEEK_API_KEY=FAKE LLME_DRY=1 ./llme.py deepseek hi             # $env 帶入 → …--api-key FAKE hi
DEEPSEEK_API_KEY=FAKE LLME_DRY=1 ./llme.py deepseek --api-key mine hi  # 使用者自帶 → 不注入
./llme.py nonexist                                                # 找不到 → 列可用 endpoint，exit 2
# 端到端（走真 pllm，用 fixture 假回應）：
./llme.py deepseek 你好 --endpoint "file://$(pwd)/pllm/test/fixtures/fake/chat/completions"
```
