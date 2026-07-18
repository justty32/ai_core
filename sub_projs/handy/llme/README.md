# llme — cllm 多 endpoint dispatcher

← [handy/AGENTS.md](../AGENTS.md)｜[INDEX](../INDEX.md)

把多組 cllm 呼叫設定收成 subcommand：`llme <endpoint> [llm 參數...]`。**不 link libcllm、不重造任何東西**——就是「挑一個 cllm config 檔、原樣轉呼現成的 [`llm`](../../cllm/README.md) CLI」。**Fennel** 本體（`_exec.fnl`）**無需建置、無外部相依**；runtime 只要 `llm` 在 PATH ＋ `fennel` 在 PATH。

> **為何 Fennel 而非編譯的 C++**：純膠水一般選編譯（零 interpreter 啟動），但 llme 的啟動**永遠緊接一次 LLM 呼叫**——那幾 ms 埋在 token 生成延遲裡＝雜訊，「零啟動」的理由對 llme 不咬。故選免建置的 Fennel。跑在 `fennel` 預設的 Lua（本機為 PUC 5.4）；luajit 的 JIT/FFI 對這種立刻 exec 掉的膠水用不到。

## 用法

```sh
llme deepseek --stream 你好   # → llm --config configs/deepseek.json --api-key $DEEPSEEK_API_KEY --stream 你好
llme deepseek 一句話介紹你自己  # deepseek ＝ 雲端 DeepSeek（configs/deepseek.json；key auto-inject）
llme local 你好                # local ＝ 本機 LM Studio（configs/local.json，無 key；目前多離線）
```

**慣例**〔提案・可改〕：`<endpoint>` 對應 `configs/<endpoint>.json`（就是一份 cllm config：`endpoint`/`model`/`api_key`/`timeout_ms`）。`_` 開頭的檔（如 `_template.json`）視為模板／隱藏，不列進可用 endpoint。其餘參數（prompt、`--stream`、`--image`、`--schema`、`--tool`…）原樣轉給 `llm`，`--config <那個檔>` 已自動帶上。

新增一個 endpoint：`cp configs/_template.json configs/opus.json` 再填（⚠ 真 config 檔**不能留 `_notes` 之類非 Client 欄位**——glaze 嚴格拒未知鍵；模板的 `_notes` 只因模板從不被載入才無事）。`api_key` 留空可走 auto-inject（見下）或 [`llm-login`](../../cllm/tools/llm-login/README.md)。

## auto-inject api key〔慣例・2026-07-18〕

呼叫 `<endpoint>` 時，使用者若**沒**自帶 `--api-key`，llme 依序找環境變數、找到就自動補 `--api-key`：

1. `LLME_KEY_<EP>`（llme 專屬覆寫）
2. `<EP>_API_KEY`（通用 provider 慣例）

`<EP>`＝endpoint 名**大寫、非英數轉 `_`**。故 `deepseek` → `DEEPSEEK_API_KEY`（對上常見 provider 命名）。

- 對應 env **沒設**（如 `local`）→ **不注入**，免 key 後端不受影響。
- 使用者**顯式** `--api-key` → 一律尊重、不覆寫。
- 用意：**keyless config 的 secret 不落版控**——config 只放 endpoint/model，key 從 env 帶。

## endpoint 清單（configs/）

| endpoint | model | key | 狀態 |
|---|---|---|---|
| `deepseek` | `deepseek-chat`（可 `--model deepseek-reasoner`）| `DEEPSEEK_API_KEY`（auto-inject）| **handy 預設後端**；已驗（見 [gotchas](../workflows/common/gotchas.md) DeepSeek 條）|
| `local` | `google/gemma-4-e4b` | 無 | 本機 LM Studio；目前多離線（省電）|
| `qwen` | `qwen3.5-9b` | 無 | 本機 LM Studio；同上 |

## 結構（folder-as-callable）

```
llme/
├─ _exec           入口薄殼（POSIX shell）——把呼叫轉給 _exec.fnl（只為讓 .fnl 拿語法高亮）
├─ _exec.fnl       Fennel 本體（dispatcher 邏輯＋auto-inject；有 shebang，也可直接跑）
└─ configs/
   ├─ deepseek.json   雲端 DeepSeek（keyless，key 靠 auto-inject）
   ├─ local.json      本機 LM Studio（無 key）
   ├─ qwen.json       本機 LM Studio
   └─ _template.json  新 endpoint 的模板
```
外層 [`../llme.sh`](../llme.sh) 是轉發入口，鏈路 `llme.sh → _exec（shell）→ _exec.fnl（Fennel）`——放上 PATH（或 symlink 成 `llme`）即可當一支命令。

## 上 PATH（讓 `llme` 變裸命令）

`llme` 轉呼的 `llm` 需在 PATH。把兩者都 symlink 進任一個 PATH 目錄（如 `~/.local/bin`）：

```sh
ln -sf <cllm>/build/llm               ~/.local/bin/llm     # 或 install-dev.sh 裝的 ~/dev/bin/llm
ln -sf <repo>/sub_projs/handy/llme.sh ~/.local/bin/llme
llme --help        # 裸命令即可用
```
`llme.sh` 與 `_exec` 都已 `readlink -f` 解 symlink，經 `~/.local/bin/llme` 呼叫也能正確定位 `_exec.fnl` 與同層 `configs/`。

## 環境變數

| 變數 | 作用 |
|--|--|
| `LLME_LLM` | 覆寫要轉呼的 `llm` 執行檔（預設 PATH 找 `llm`；**測試設 `echo`** 可看轉出的參數而不真打） |
| `LLME_CONFIG_DIR` | 覆寫 config 目錄（預設＝`_exec.fnl` 同層的 `configs/`） |
| `LLME_KEY_<EP>` / `<EP>_API_KEY` | auto-inject 的 api key 來源（見上「auto-inject」）|

## 冒煙自測（不需真後端）

```sh
LLME_LLM=echo ./_exec local --stream 你好                    # 印：--config …/local.json --stream 你好（local 無 key，不注入）
DEEPSEEK_API_KEY=FAKE LLME_LLM=echo ./_exec deepseek hi       # 印：… --api-key FAKE hi（auto-inject）
DEEPSEEK_API_KEY=FAKE LLME_LLM=echo ./_exec deepseek --api-key mine hi  # 使用者自帶 → 只 mine、不注入
LLME_LLM=echo ./_exec nonexist                               # 找不到 → 列可用 endpoint，exit 2
LLME_LLM=false ./_exec local x                               # 退出碼傳遞：llm 回幾 llme 就回幾（此處 1）
./_exec --help                                               # 用法＋可用 endpoint
```
已驗：正常轉發／中文＋空白＋含 `'` 參數的 shell 安全轉義／auto-inject 四情境／找不到 endpoint／usage／`--help`／退出碼傳遞／經 `llme.sh` 與 `_exec` 兩段 shim 跨 cwd。
