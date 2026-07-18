# llme — cllm 多 endpoint dispatcher

← [handy/AGENTS.md](../AGENTS.md)

把多組 cllm 呼叫設定收成 subcommand：`llme <endpoint> [llm 參數...]`。**不 link libcllm、不重造任何東西**——就是「挑一個 cllm config 檔、原樣轉呼現成的 [`llm`](../../cllm/README.md) CLI」。純 C++17 一個檔（`main.cpp`），建置零依賴 cllm；runtime 只要 `llm` 在 PATH。

## 用法

```sh
llme opus --stream 你好      # → llm --config configs/opus.json --stream 你好
llme local 一句話介紹你自己   # local ＝ 本機 LM Studio（configs/local.json）
```

**慣例**〔提案・可改〕：`<endpoint>` 對應 `configs/<endpoint>.json`（就是一份 cllm config：`endpoint`/`model`/`api_key`/`timeout_ms`）。`_` 開頭的檔（如 `_template.json`）視為模板／隱藏，不列進可用 endpoint。其餘參數（prompt、`--stream`、`--image`、`--schema`…）原樣轉給 `llm`，`--config <那個檔>` 已自動帶上。

新增一個 endpoint：`cp configs/_template.json configs/opus.json` 再填。`api_key` 留空可走 [`llm-login`](../../cllm/tools/llm-login/README.md)。

## 結構（folder-as-callable）

```
llme/
├─ main.cpp        轉發器原始碼
├─ _exec           編出來的可執行體（0708 的 _exec；.gitignore，需自建）
├─ CMakeLists.txt  建置（產物 _exec 落在本資料夾）
└─ configs/
   ├─ local.json      本機 LM Studio（無 key）
   └─ _template.json  新 endpoint 的模板
```
外層 [`../llme.sh`](../llme.sh) 是轉發入口，把呼叫轉給本資料夾的 `_exec`——放上 PATH（或 symlink 成 `llme`）即可當一支命令。

## 建置

```sh
c++ -std=c++17 -O2 main.cpp -o _exec      # 一行；或用 CMake：
cmake -B build && cmake --build build      # 產物同樣落在本資料夾的 _exec
```

## 環境變數

| 變數 | 作用 |
|--|--|
| `LLME_LLM` | 覆寫要轉呼的 `llm` 執行檔（預設 PATH 找 `llm`；**測試設 `echo`** 可看轉出的參數而不真打） |
| `LLME_CONFIG_DIR` | 覆寫 config 目錄（預設＝`_exec` 同層的 `configs/`） |

## 冒煙自測（不需真後端）

```sh
LLME_LLM=echo ./_exec local --stream 你好   # 印：--config …/local.json --stream 你好
LLME_LLM=echo ./_exec nonexist              # 找不到 → 列可用 endpoint，exit 2
./_exec --help                              # 用法＋可用 endpoint
```
