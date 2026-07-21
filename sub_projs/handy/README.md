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

> ⚠ `util.llm` 依賴 **litellm**（外部套件），handy 其餘部分維持零相依。**本輪是試裝試打的測試階段**，
> 不是拍板的架構決定——去留待試出體感再談，見 [SESSION-LOG](SESSION-LOG.md)。

---

## util — 通用共用 lib

「很多 `.py` 共用的小工具庫」——純函式、無副作用、不綁特定住戶。**Python 隱式命名空間套件**，
往裡丟一個 `.py` 就多一個子模組。

| 模組 | 用途 |
|---|---|
| `util.config` | 讀 JSON ＋ 解析 `$env`／`$ref`（含跨檔引用與陣列合併）|
| `util.jsref` | `$ref` 語法的純函式：拆解／走路徑／深合併 |
| `util.env` | 環境變數小工具：`first()`／`stem()`（⚠ 目前無人使用）|
| `util.llm` | **LLM 地基**：litellm 包裝，轉出 `ask`／`cli_main`／`LLMError` |

**用法、實測行為表、住戶怎麼 import 一律見 → [util/README.md](util/README.md)。**

---

## util.llm — LLM 地基（住戶的「腦」）

litellm 的薄包裝，**對外形狀＝已封存的 pllm**：`ask()` 簽章與整套 CLI 旗標逐項照舊，換腦不換介面。

```python
from util.llm import ask          # 函式庫用法
from util.llm import cli_main     # 借用整套 CLI（llme 就是這樣透傳）
```

**完整用法、安裝（venv／litellm 版本釘選）、結構、以及七個實測過的坑
（`drop_params` 靜默丟參數、api_key 必給、`openai/` 開頭模型名被吃掉…）
一律見 → [util/llm/README.md](util/llm/README.md)。**

```sh
pip install litellm                  # ⚠ 唯一外部相依
python util/llm/test/smoke.py        # 離線冒煙（不連網、不需 litellm）；現況 49/49
```

---

## llme — 多 endpoint dispatcher（住戶①）

**換 endpoint 名＝換模型**：`llme <endpoint> [util.llm 的其餘參數...]`。從 `llme.json` 挑一組設定、
翻成連線旗標，其餘參數原樣透傳，再**重用 `util.llm` 自己的 CLI 解析**跑一次（不 subprocess）。

```sh
./llme.py deepseek 一句話介紹你自己      # 雲端 DeepSeek（需 DEEPSEEK_API_KEY）
./llme.py openrouter 你好                # OpenRouter 免費模型（需 OPENROUTER_API_KEY）
./llme.py deepseek --stream 你好         # 透傳 --stream 等所有 util.llm 旗標
./llme.py local 你好                     # 本機 LM Studio（無 key；目前多離線）
```

上 PATH 當裸命令：`ln -sf "$(pwd)/llme.py" ~/.local/bin/llme`（有 shebang，直接可執行）。

**刻意極簡**：整支 36 行、一個函式，就是「讀 json → 查 endpoint → 填旗標 → 併 argv → 轉發」。
沒有 `--help`、沒有錯誤訊息、沒有設定檔路徑覆寫——**打錯 endpoint 名就是 `KeyError` traceback**。
要看有哪些 endpoint 直接開 `llme.json`；要看旗標用 `./llme.py <任一ep> --help`（那是 `util.llm` 的）。

### llme.json（多模型定義在一檔）

`<endpoint 名> → { 欄位: 值, ... }`。**欄位名就是 `util.llm` 的旗標名，底線換連字號**
（`timeout_ms` → `--timeout-ms`），所以想固定 `temperature`、`max_tokens` 之類的，直接加一欄就好，
**不必回頭改 llme.py**。慣用四欄：`endpoint`／`model`／`timeout_ms`／`api_key`。

用 `util.config` 讀，故吃 `$env`/`$ref`：金鑰寫成 `{"$env":"DEEPSEEK_API_KEY"}`、共用端點用
`{"$ref":"#_lmstudio"}`（`#` 開頭＝本檔片段）；`_` 開頭鍵非 endpoint（當共用資料）。**加模型＝在此加一列**。

> ⚠ 因為欄位直接變旗標，**打錯欄位名＝`util.llm` 報「未知旗標」**（不是被忽略）。

### api_key 來源

金鑰**只有一個來源**：`llme.json` 該 endpoint 的 `api_key`（通常寫成 `{"$env":"…"}`，由 `util.config`
從環境變數帶入）。本地免 key 的 endpoint 不填即可。

使用者在命令列自帶 `--api-key` 仍會生效並蓋過設定檔——不是 llme 特別處理，而是**透傳參數排在後面，
`util.llm` 的 argv 解析後者覆寫前者**，自然如此。

### 冒煙自測（不需真後端）

```sh
# 端到端（走真 util.llm，用 fixture 假回應、不連網）：
./llme.py local 你好 --endpoint "file://$(pwd)/util/llm/test/fixtures/text.json"
```

> llme 沒有自己的測試套件——它就是「查表→翻旗標→轉發」，翻譯與解析的測試都在
> [util/llm/test/smoke.py](util/llm/test/smoke.py)。
