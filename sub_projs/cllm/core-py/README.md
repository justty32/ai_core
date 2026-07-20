# core-py/ — cllm 的純 Python 平行實作

← [cllm 傘](../README.md)

這是 cllm 的**純 Python 版**：與 C++ 的 [`core/`](../core/)（`libcllm` 共享庫 + `llm` CLI）**並列**，
做同一件事——把 prompt 組成 OpenAI 相容的 `/chat/completions` 請求、打出去、解回應——但
**完全不碰 C ABI、不載 `.so`/`.dll`**，而是用 Python 標準庫直接打 HTTP。

## 與 `core/` 的關係

| | `core/`（C++） | `core-py/`（本目錄） |
|---|---|---|
| 對外 | `libcllm` 共享庫（`llm_ask`）＋ `llm` CLI 執行檔 | 可 import 的 `cllm` package ＋ `cllm` CLI |
| 傳輸 | WinHTTP／libcurl，經 C ABI | Python `urllib`，直接打 HTTP |
| 相依 | CMake + Ninja + vcpkg + glaze | 零外部相依（純標準庫） |
| 真相源 | 是（請求／回應語意的定義處） | 對齊 `core/`，不另立語意 |

`core-py` 的行為刻意**鏡像** `core/`：請求 JSON 逐欄對齊 `core/src/cabi_request.cpp`、CLI 旗標與
prompt/stdin 合體規則對齊 `core/src/cli.cpp`＋`cli_flags.cpp`、內核 `ask()` 的 API 形狀對齊既有
ctypes binding `core/bindings/python/llm.py`（差別只在「不經 ctypes、直接打 HTTP」）。離線自測
直接複用 `core/test/fixtures/` 的假回應，好與 C++ 版黑箱比對。

## 結構

```
core-py/
├── cllm/
│   ├── __init__.py   # re-export ask / LLMError
│   ├── core.py       # ★ 內核（可 import）：組請求、打 HTTP、解串流、跑回呼
│   ├── cli.py        # 薄 CLI 外殼（cli.cpp）：main 解析＋接線、_entry
│   ├── internal.py   # 共用底（cli_internal.hpp）：退出碼／env 鍵／檔案讀取
│   ├── flags.py      # 反射旗標表＋print_usage（cli_flags.cpp）
│   ├── config.py     # 三層 config 解析（cli_config.cpp）
│   ├── media.py      # --media 三分流取值＋MIME 對照
│   ├── output.py     # 出口 handlers 收成 Sink（cli_output.cpp）
│   └── __main__.py   # python -m cllm 進入點
├── test/
│   └── cli_smoke.py  # 離線黑箱煙霧測試（走 file:// 假回應，仿 cli_smoke.sh）
├── pyproject.toml
└── README.md
```

**套件名選 `cllm`**（package），內核在 `cllm/core.py`；CLI 外殼 `cllm/cli.py` 只留 main 解析＋接線，
周邊按 C++ 的分檔拆到姊妹模組（`internal`/`flags`/`config`/`media`/`output`，括號內為對齊的 C++ 檔）。

## 相依取捨

本子專案的相依政策比 ai_core 主 repo 放寬——**可以用 PyPI 套件**（見 [cllm 對本目錄的
CLAUDE.md](../CLAUDE.md)）。但權衡後選擇**零外部相依**：工作量（單次 `POST` ＋ SSE 逐行拆框）
標準庫 `urllib`/`json` 就乾淨覆蓋，換得零安裝、跨平台、與 C ABI 那條「小、免開發環境」的敘事
一致；CLI 的旗標／prompt 交錯規則也比 `argparse` 的模型自由，手寫解析反而更貼合 `llm`。若日後
要連線池／HTTP2／重試策略，再引 `httpx` 才划算。

## 安裝

不裝也能跑（見下「直接跑」）。要裝成指令：

```sh
pip install -e .
```

會註冊 `cllm` 與 `llm` 兩個同名指令（用法鏡像 C++ 的 `llm` CLI）。
⚠ `llm` 與 `core/` 建出的原生 `llm` 執行檔可能撞名——想避開就只用 `cllm`／`python -m cllm`，或用
虛擬環境隔離。

## 當內核 import

```python
from cllm import ask

text = ask("你好")                                      # 只給 prompt（走內建 localhost）
text = ask("你好", "http://…/chat/completions")         # prompt + endpoint（位置形式）
text = ask("你好", model="local-model", temperature=0.7)

ask("數到五", stream=True,                              # 串流：逐段進 on_delta
    on_delta=lambda piece: print(piece, end="", flush=True))   # 回真值可中止

# tools / media / modalities（皆為 kwargs，可任意組合）
ask("東京天氣如何？",
    tools=[{"name": "get_weather", "description": "查天氣",
            "parameters": '{"type":"object","properties":{"city":{"type":"string"}}}'}],
    on_tool=lambda call: print(call["name"], call["arguments"]))  # call={id,name,arguments}
```

`ask()` 回**完整答案字串**；失敗時：給了 `on_error` 就呼叫它並回 `None`，否則 `raise LLMError`。
串流走 `on_delta` 逐段；回呼回真值＝中止，中止後回「已收到的部分」。完整簽章與陷阱見
`cllm/core.py` 的模組 docstring。

## CLI 用法

用法大致與 `llm` CLI 一樣（旗標、prompt/stdin 合體規則、`--` 分隔符、`-` stdin 插入點都鏡像），
但**四個旗標的取值語意刻意與 C++ `llm` 分歧**（見下「⚠ 與 C++ llm 的分歧」）：

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

### ⚠ 與 C++ llm 的分歧（只發生在 core-py）

**`--schema` / `--tool` / `--modality` 的 cfg：收「字面 JSON 文字」，不再讀檔**（與 `--system`
同款）。要吃檔案內容一律靠 shell `$(cat …)`：

```sh
python -m cllm 給我角色 --schema '{"type":"object","required":["name"]}'
python -m cllm 給我角色 --schema "$(cat schema.json)"          # 檔案內容靠 $(cat)
python -m cllm 東京天氣 --tool '{"name":"get_weather","description":"查天氣","parameters":{…}}'
python -m cllm 說你好 --modality 'audio={"voice":"alloy"}'      # 名=<字面JSON>；只給 audio＝無參數
```

後果（已接受）：`--schema s.json` 這種「給路徑」用法**停用**——路徑會被當字面文字、解 JSON
失敗回退出碼 1。長內容一律 `$(cat …)`。

**`--image`／`--media`（兩者同義）：例外保留讀檔，並按值的樣子三分流**：

| 值的樣子 | 行為 |
|---|---|
| `data:` 或 `http(s)://` URL | 直接當 media 的 `url` 送、**不編碼** |
| 以 `.json` 結尾（用副檔名判定） | 讀檔 `json.loads`，當**預先算好的 media 描述子**直通、**不重算 base64**：`{"url":"data:…"}` 或 `{"mime":"…","data":"<base64>"}`；形狀不符／JSON 壞 → 退出碼 1 |
| 其餘（二進位圖檔路徑） | 讀檔 + base64 編碼（**現行行為，不變**；mime 由副檔名推） |

```sh
python -m cllm 描述這張圖 --media photo.png                    # 二進位：讀檔+base64
python -m cllm 描述這張圖 --media 'data:image/png;base64,…'    # URL：直通不編碼
python -m cllm 描述這張圖 --media precomputed.json             # 描述子：直通，省重算
```

設定來源（後者覆寫前者）：內建預設 → config 檔 → 命令列旗標。config 檔路徑：
`--config <檔>` ＞ 環境變數 `LLM_CLI_CONFIG` ＞ `~/.config/llm/config.json`。

退出碼：`0` 成功；`1` 用法錯；`2` 請求失敗（傳輸／後端／媒體落檔失敗）；`130` 取消。

## 如何離線自測

走 `file://` 假回應，不連網、不吃真後端。直接複用 `core/test/fixtures/`：

```sh
python test/cli_smoke.py
```

真的用 `subprocess` 跑 CLI、驗 stdout／退出碼（黑箱），全過回 0、任一敗回 1。Windows + POSIX
雙可跑（`file://` URL 由 fixtures 絕對路徑拼成，內核 `_file_url_to_path` 兩平台都解得動）。

離線 fixture 驗得到「解析／組裝／退出碼分流」，驗不到「後端錯誤語意」（假回應永遠 200、欄位齊全）
——那要真後端才驗得到。手動指向假回應也行：

```sh
python -m cllm 你好 --endpoint "file:///絕對路徑/core/test/fixtures/fake/chat/completions"
```

## 已知落差

- **SIGINT／cancel**：C++ 版在 POSIX 用 SIGINT 中途取消在途請求（退出碼 130）；本版只在
  `python -m cllm` 進入點包 `KeyboardInterrupt → 130`，非串流的 `urllib` 阻塞讀無法像 C++
  串流那樣「每塊查旗標乾淨斷」。回呼回真值的**主動中止**（回已收部分）則完全對齊。
- **config 未知鍵**：C++ glaze 預設對未知鍵報錯；本版對未知鍵**寬鬆忽略**（只挑認得的欄位），
  壞 JSON 仍照樣回退出碼 1。
- **四旗標取值語意**（刻意分歧，非 bug）：`--schema`／`--tool`／`--modality` 的 cfg 收字面 JSON
  而非檔案路徑；`--image`／`--media` 三分流（URL 直通／`.json` 描述子直通／二進位讀檔編碼）。
  詳見上「⚠ 與 C++ llm 的分歧」。C++ `core/` 那條線維持原「讀檔」語意不受影響。
