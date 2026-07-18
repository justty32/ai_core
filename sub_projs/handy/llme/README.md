# llme — cllm 多 endpoint dispatcher

← [handy/AGENTS.md](../AGENTS.md)

把多組 cllm 呼叫設定收成 subcommand：`llme <endpoint> [llm 參數...]`。**不 link libcllm、不重造任何東西**——就是「挑一個 cllm config 檔、原樣轉呼現成的 [`llm`](../../cllm/README.md) CLI」。純 **Fennel** 一個檔（`_exec`），**無需建置、無外部相依**；runtime 只要 `llm` 在 PATH ＋ `fennel` 在 PATH。

> **為何 Fennel 而非編譯的 C++**：純膠水一般選編譯（零 interpreter 啟動），但 llme 的啟動**永遠緊接一次 LLM 呼叫**——那幾 ms 埋在 token 生成延遲裡＝雜訊，「零啟動」的理由對 llme 不咬。故選免建置的 Fennel（直接丟進 bag、也當 handy 的 LuaJIT/Fennel 工具鏈試金石）。跑在 `fennel` 預設的 Lua（本機為 PUC 5.4）；luajit 的 JIT/FFI 對這種立刻 exec 掉的膠水用不到。

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
├─ _exec           Fennel 轉發器（0708 的 _exec 執行體；有 shebang、chmod +x，直接可跑）
└─ configs/
   ├─ local.json      本機 LM Studio（無 key）
   └─ _template.json  新 endpoint 的模板
```
外層 [`../llme.sh`](../llme.sh) 是轉發入口，把呼叫轉給本資料夾的 `_exec`——放上 PATH（或 symlink 成 `llme`）即可當一支命令。

## 環境變數

| 變數 | 作用 |
|--|--|
| `LLME_LLM` | 覆寫要轉呼的 `llm` 執行檔（預設 PATH 找 `llm`；**測試設 `echo`** 可看轉出的參數而不真打） |
| `LLME_CONFIG_DIR` | 覆寫 config 目錄（預設＝`_exec` 同層的 `configs/`） |

## 冒煙自測（不需真後端）

```sh
LLME_LLM=echo ./_exec local --stream 你好   # 印：--config …/local.json --stream 你好
LLME_LLM=echo ./_exec nonexist              # 找不到 → 列可用 endpoint，exit 2
LLME_LLM=false ./_exec local x              # 退出碼傳遞：llm 回幾 llme 就回幾（此處 1）
./_exec --help                              # 用法＋可用 endpoint
```
已驗：正常轉發／中文＋空白＋含 `'` 參數的 shell 安全轉義／找不到 endpoint／usage／`--help`／退出碼傳遞／經 `llme.sh` 跨 cwd。
