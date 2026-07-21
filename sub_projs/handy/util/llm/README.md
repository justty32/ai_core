# util.llm — handy 的 LLM 地基（litellm 包裝）

← [handy](../../)｜[SESSION-LOG](../../SESSION-LOG.md)

住戶拿 LLM 當「腦」就吃這裡。內臟是 **litellm**，但**對外形狀＝已封存的 [pllm](../../archive/pllm/)**：
`ask()` 簽章與整套 CLI 旗標逐項照舊，**換腦不換介面**。

> **本輪是試裝試打的測試階段**，不是拍板的架構決定；litellm 的去留待試出體感再談。

---

## 怎麼用

### 當函式庫

```python
from util.llm import ask

text = ask("你好")                                    # 走內建 localhost endpoint
text = ask("你好", "https://…/v1/chat/completions", model="deepseek-chat")
ask("數到五", stream=True, on_delta=lambda s: print(s, end="", flush=True))
ask("東京天氣？", tools=[{"name": "get_weather", "description": "查天氣",
                        "parameters": '{"type":"object","properties":{"city":{"type":"string"}}}'}],
    on_tool=lambda c: print(c["name"], c["arguments"]))   # c={id,name,arguments}
```

`ask()` 回**完整答案字串**；失敗時給了 `on_error` 就呼叫它並回 `None`，否則 `raise LLMError`。
回呼回真值＝**要求中止**，中止後回「已收到的部分」。完整簽章見 [core.py](core.py) 的 docstring。

住戶要 import，把 `sys.path` 指到 **handy 根**（`util` 的父層）：

```python
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))   # 住戶在 handy 根時
from util.llm import ask
```

### 當 CLI

```python
from util.llm import cli_main       # 借用整套 CLI（llme.py 就是這樣透傳）

cli_main(["llm", "--stream", "你好"])   # 回退出碼
cli_main()                              # 省略＝取 sys.argv
```

> ⚠ argv **收整份 `sys.argv`**（含程式名、從第 1 項開始解析），沿用 pllm 慣例。自己組 argv 呼叫時
> 記得墊一個第 0 項——llme.py 墊的就是字串 `"llme"`。

實際多半透過住戶 [`llme`](../../README.md#llme--多-endpoint-dispatcher住戶) 用：

```sh
./llme.py openrouter 你好
./llme.py openrouter 數到五 --stream
./llme.py <ep> --help            # 完整旗標說明
```

**旗標**：`--system` / `--stream` / `--image`(`--media`) / `--schema` / `--config` / `--tool` /
`--modality` / `--media-out` / `--` / `--help`，加連線取樣類 `--endpoint` / `--api-key` /
`--timeout-ms` / `--model` / `--temperature` / `--top-p` / `--presence-penalty` /
`--frequency-penalty` / `--max-tokens` / `--seed`。

**退出碼**：`0` 成功｜`1` 用法錯｜`2` 請求失敗｜`130` 取消。

**prompt 來源**：位置參數＋導管 stdin 可合體——`-` ＝ stdin 插入點；沒寫 `-` 而兩者都有＝
prompt＋空行＋stdin；只給其一＝用那一個。

---

## ⚠ 坑（全部實測過，別重踩）

### 1. `drop_params=True`：不支援的參數會被**靜默丟掉**

[call.py](call.py) 開了 litellm 的 `drop_params`，後端不吃的取樣參數會被自動丟棄**而非報錯**。
好處是參數不合的後端不會當場炸；**代價是你不會知道某個參數被無聲吃掉了**。

**實例**：拿 `--schema` 打 OpenRouter 的 `cohere/north-mini-code:free`，模型回的是散文＋markdown
包的 JSON，不是嚴格結構化輸出。請求組裝本身正確（對假後端比對過，`response_format.json_schema`
欄位完整送出），推測就是 litellm 認定該後端不支援而丟掉了。

> 想要「不支援就炸給我看」而非靜默失效，把 `call.py` 的 `litellm.drop_params = True` 改掉即可。
> 這是雙面刃，尚未定案。

### 2. 沒給 api_key 會**完全打不出去**（已修，但要知道為何）

litellm 的 openai provider **一律要求 `api_key`**，不給就當場丟 `InternalServerError:
Missing credentials`——**連本機 LM Studio 這種免認證端點都送不出去**。舊 pllm 是「沒 key 就不送
`Authorization` header」，故無此限。

**現行修法**：沒給 key 時補佔位字串 `handy-no-auth`（免認證後端會忽略）。有迴歸測試守門。

### 3. model 前綴：`openai/` 開頭的模型名會**被吃掉一層**

handy 只打 OpenAI-compatible 端點，所以 [call.py](call.py) 預設幫 model **加 `openai/` 前綴**，
litellm 送出前再剝掉——一來一回剛好還原，`google/gemma-4-e4b` 這種含斜線的名字也不會被誤判成
provider。

**但模型名本身就以 `openai/` 開頭時（OpenRouter 真的有，如 `openai/gpt-oss-20b:free`）就壞了**：

| 你寫的 `--model` | 實際送到後端的 model | 對嗎 |
|---|---|---|
| `google/gemma-4-e4b` | `google/gemma-4-e4b` | ✅ |
| `openai/gpt-oss-20b:free` | `gpt-oss-20b:free` | ❌ 少一層 |
| `openai/gpt-oss-20b:free` ＋ `LLM_RAW_MODEL=1` | `gpt-oss-20b:free` | ❌ **救不了** |
| `openai/openai/gpt-oss-20b:free` | `openai/gpt-oss-20b:free` | ✅ **這是解法** |

⚠ **`LLM_RAW_MODEL=1` 對這個情況無效**——它只擋住 handy 這邊加前綴，litellm 自己仍會剝掉開頭的
`openai/`。要送出字面以 `openai/` 開頭的名字，**手動寫成雙前綴**。

### 4. `--endpoint` 收**完整 URL**，內部才剝成 api_base

舊介面不動：`--endpoint https://host/v1/chat/completions`。[call.py](call.py) 的 `api_base()`
剝掉尾巴的 `/chat/completions` 交給 litellm。**別自己傳半截 URL**，兩邊語意不同。

### 5. `file://` 逃生口只解**非串流**

`--endpoint file://<絕對路徑>` 不經 litellm，直接讀該檔當一份**非串流**回應 JSON，供無後端自測。
串流的 SSE fixture **不支援**（要測串流請起真後端，見下）。Windows 路徑直接寫 `file://C:/…`。

### 6. stdin 在「非 tty 又沒人餵」時會**卡住**

CLI 只要 stdin 不是互動終端就整段讀到 EOF。在 CI／`bash -c` 這類 stdin 既非 tty 又沒東西餵的環境
會**永久阻塞**。這是沿用 pllm 的行為，非新 bug——測試時一律帶 `< /dev/null`。

### 7. OpenRouter 免費 slug 汰換很快

`tencent/hy3:free` 已轉付費、`google/gemma-4-31b-it:free` 上游限流。用之前先查
`https://openrouter.ai/api/v1/models` 挑當下真的免費的（`pricing.prompt` 與 `.completion` 皆 0）。

---

## 相依與安裝

**唯一外部相依＝litellm**。沒裝時打真端點會報「litellm 未安裝」（`import util.llm`、`--help`、
離線自測都**不需要**它——[call.py](call.py) 是延遲載入）。

```sh
pip install litellm
```

**本機（Windows 開發機）已備好 venv**：`handy/.venv`（版控忽略）。建法：

```sh
uv venv --python "C:/Users/WG-Guanyu/AppData/Local/Programs/Python/Python313/python.exe" .venv
uv pip install --python ./.venv/Scripts/python.exe --only-binary :all: litellm
./.venv/Scripts/python.exe llme.py openrouter 你好
```

兩個非做不可的細節（血淚見 [SESSION-LOG](../../SESSION-LOG.md) 的「開發環境」節）：

- **解釋器必用官方 python.org 簽名版**：uv 自管的 standalone 版其 openssl DLL 被本機應用程式控制
  策略擋掉 → `import ssl` 失敗 → 連不了任何 https。
- **`--only-binary :all:` 不能省**：讓 resolver 退到 **litellm 1.93 之前**（實裝 1.91.4），避開
  1.93+ 自帶的 `litellm-rust`（需 cargo 現場編譯）。升級前先確認這點。

---

## 自測

```sh
python util/llm/test/smoke.py     # 離線冒煙：不連網、不需 litellm；現況 36/36
```

驗得到「argv 解析／退出碼分流／參數翻譯／回應解讀」；**驗不到**「litellm 真的打得通」與「後端錯誤
語意」。要驗那些就起真後端——`test/` 沒收假後端腳本，需要時照 [SESSION-LOG](../../SESSION-LOG.md)
的紀錄用 stdlib `http.server` 現寫一個（能同時吐 JSON 與 SSE 即可）。

**已對真後端驗過**（2026-07-21，OpenRouter `cohere/north-mini-code:free`）：非串流／串流／
`--system`／`--tool`（真往返）／stdin 合體全過；`--schema` 見坑 1。

---

## 結構

每檔**單一職責、≤150 行、行 ≤80 字元**，依賴為無環 DAG（`errors` 是葉）。

```
util/llm/
├─ __init__.py   門面：re-export ask / cli_main / LLMError / DEFAULT_ENDPOINT
├─ core.py       ★ ask() 門面＋回呼接線
├─ call.py       去程：參數 → litellm kwargs（★坑 1/2/3/4 都在這）
├─ msg.py        去程：組 messages（system／user＋輸入媒體）
├─ resp.py       回程：litellm 回應 → 四個回呼（fixture 也走這條）
├─ errors.py     葉：LLMError／預設端點／退出碼
├─ cli.py        CLI 主接線：prompt 合體＋設定疊加＋呼叫 ask
├─ argv.py       argv 掃描 → ParsedArgs
├─ flags.py      反射旗標表＋--help 用法文字
├─ cfg.py        config 檔那層（⚠ 與 util.config 無關，那是 $env/$ref 讀取器）
├─ req.py        --schema/--tool/--modality 驗證組裝
├─ media.py      --image/--media 的 MIME 對照與三分流取值
├─ out.py        出口 Sink：文字/tool_calls/媒體落檔/錯誤
└─ test/         離線冒煙＋fixtures
```

**去程／回程**是理解本套件的主軸：`call.py`＋`msg.py` 把 pllm 形狀翻成 litellm 形狀，`resp.py` 翻回來。
要改「送什麼」看去程，要改「怎麼解讀回來的東西」看回程。

### 沿用 pllm 的語意（沒變的部分）

- `--schema` / `--tool` / `--modality` 收**字面 JSON 文字、不讀檔**（同 `--system`）；要吃檔案內容
  一律 `$(cat s.json)`。
- `--image` / `--media` **例外保留讀檔**，並按值三分流：`data:`／`http(s)://` URL 直通不編碼；
  `.json` 結尾當預先算好的描述子直通；其餘當二進位圖檔，讀檔＋base64（mime 由副檔名推）。
- 設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。config 檔路徑：`--config` ＞
  環境變數 `LLM_CLI_CONFIG` ＞ `~/.config/llm/config.json`。

### 環境變數

| 變數 | 作用 |
|---|---|
| `LLM_CLI_CONFIG` | 指定 config 檔路徑（只指路徑，不存設定值）|
| `LLM_RAW_MODEL=1` | 關掉 handy 這邊加 `openai/` 前綴（⚠ 對坑 3 無效）|
