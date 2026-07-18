# hermy（Fennel / Lua 版）— curl+jq shell-out 移植

← [hermy（Python 原版）](../../README.md)｜[handy/AGENTS.md](../../../AGENTS.md)

[hermy](../../README.md)（用 handy 思想復刻 hermes-agent 的最小自我改進技能型 agent）的
**Fennel 移植**，逐檔與 Python 原版 **1:1 對齊行為**。Fennel 編到 **Lua**（`#!/usr/bin/env fennel`，
本機 `fennel` 1.6.1 on PUC Lua 5.4）。**不引任何 Lua JSON/HTTP 庫**——JSON 靠 `jq`、HTTP 靠 `curl`。

## 本質映射（同原版）

| hermes 核心 | hermy 的 handy 化 |
|---|---|
| **Skills**（目錄＋腳本＋LLM 工具、`create_skill` 自產）| **folder-as-callable**：`skills/<name>/`＝`skill.json`＋`run`。`create_skill`＝AI 自我擴展 |
| **對話迴圈**（模型→工具分派→結果→迴圈）| DeepSeek function-calling 迴圈，工具＝shell-out 呼叫技能 `run` |
| **記憶**（對話後同步、檔案化）| `memory/log.ndjson` append-log |
| **LLM provider** | DeepSeek（`deepseek-chat`）|

## 與 Python 原版的差異（＝本移植的重點）

- **HTTP 靠 `curl`、JSON 靠 `jq`**：Lua 標準庫無 JSON／HTTP，全部 shell-out 給現成 unix 引擎，
  對齊 handy「拿現成程式用腳本包」——與 `ports/{cl,lua}` 同策略。
  - `ds-chat` = `jq`（組 request body、抽 `.choices[0].message`）+ `curl`（POST，`-w '\n%{http_code}'` 取狀態碼）。
  - `skills-json` / `mem-append` / `hermy` 的 messages 增修全走 `jq`。
- **messages 陣列存成 JSON 字串**，用 `jq` 增修（`. + [$msg]`、`--argjson` 帶入），大陣列一律走
  **stdin** 餵 jq（避開 Linux 單一 argv 上限）。
- **CLI 薄殼各 6 行、邏輯全在庫**：每個可執行檔（`hermy`＋`lib/*`）只做「設 `fennel.path`＋
  `(require :util)`＋呼叫一個 main」，**≤10 行**；所有邏輯都在 `src/util.fnl`（庫，204 行，不限）。

## Fennel 模組載入（本移植挑的招）

Fennel 檔被 `require` 時，會拿到 `(模組名 檔路徑)` 兩個 vararg——**這就是 Lua 版的 `*load-truename*`**。
`src/util.fnl` 頂端 `(local (_modname mypath) ...)` 抓到自己的路徑，往上兩層算出 `*root*`，
**與哪個 CLI load 它無關**（複製 CL 版庫自算根的優雅）。薄殼只負責找到庫：

```fennel
(local fennel (require :fennel))
(local here (: (. arg 0) :match "^(.*)/[^/]*$"))
(set fennel.path (.. here "/src/?.fnl;" here "/../src/?.fnl;" fennel.path))
((. (require :util) :orchestrate))
```

`fennel.path` 同時加 `here/src`（給根層的 `hermy`）與 `here/../src`（給 `lib/*`），故**五個薄殼一字不差**、
`require :util` 都找得到同一份庫。庫自算根，路徑裡的 `..` 交給 OS 解析（skills/memory/prompt 都併在 root 下）。

> 為何不用 `fennel.dofile`？`dofile` 每次重編、且不會把「自身路徑」餵進模組。`require` 有快取＋
> 標準 vararg 傳路徑，最穩、最少行——故選它。

## Lua 陷阱（都在 `src/util.fnl` 解掉）

- **`gsub` 回兩值**：`trim` / `shq` 用 `(pick-values 1 (: s :gsub ...))` 截一值，否則多回的計數會污染串接。
- **`io.popen` 單向**：要同時餵 stdin＋抓 exit code，一律把 stdin 寫**暫存檔**、
  `cmd < inf > of 2> ef; echo $? > cf` 各接一檔再讀回（見 `run`）。
- **自寫 `shquote`**：`shq` 把字串包單引號、內部 `'`→`'\''`，所有 shell-out 參數（含技能名、jq `--arg`）都過它。

## 用法

```sh
export DEEPSEEK_API_KEY=sk-...
./hermy 用 shell 技能看看當前資料夾有哪些檔案
echo "把 37 度攝氏轉華氏" | ./hermy
```

## 結構（每檔可單獨 CLI 呼叫）

```
ports/fennel/
├─ hermy                CLI 薄殼（6 行）→ (util.orchestrate)：agent 迴圈
├─ lib/                 CLI 薄殼（各 6 行），可單獨呼叫
│  ├─ skills-json       → (util.skills-json)  掃 skills/ → OpenAI tools JSON（「發現」）
│  ├─ ds-chat           → (util.ds-chat)      stdin{messages,tools} → curl 一次 DeepSeek → message（「腦」）
│  ├─ run-skill <name>  → (util.run-skill)    stdin=參數 → exec skills/<name>/run（「手」）
│  └─ mem-append        → (util.mem-append)   stdin=record → append memory/log.ndjson（「記憶」）
├─ src/util.fnl         ★ 庫（204 行）：所有邏輯 + shell-out/jq/env/I/O 助手，自算 *root*
├─ prompt/system.md     system prompt（資料）
├─ skills/              技能庫（種子 shell/、create_skill/ 沿用原版，run 可為任何語言）
└─ memory/             記憶（log.ndjson，runtime、不進版控）
```

單獨測（免 API）：`./lib/skills-json | jq length`、`echo '{"command":"ls"}' | ./lib/run-skill shell`。

**技能契約**（同原版）：`skills/<name>/skill.json`＝`{"name","description","parameters":<JSON schema>}`；
`skills/<name>/run`＝可執行、**從 stdin 收 JSON 參數、印結果於 stdout**。`_` 開頭資料夾忽略。

## 環境變數（同原版）

| 變數 | 預設 | 作用 |
|--|--|--|
| `DEEPSEEK_API_KEY` | （必給）| DeepSeek 金鑰 |
| `HERMY_MODEL` | `deepseek-chat` | 模型 |
| `HERMY_ENDPOINT` | `https://api.deepseek.com/v1/chat/completions` | endpoint |
| `HERMY_MAX_STEPS` | `12` | 工具迴圈上限 |
| `HERMY_SKILLS_DIR` / `HERMY_MEMORY_DIR` | 同層 `skills/`／`memory/` | 覆寫目錄 |

## 細節偏差

- `iso-now` 為秒級 UTC（原版 Python `isoformat()` 帶微秒），與 CL 版一致。
- **symlink**：薄殼從 `arg[0]` 算路徑、**不解 symlink**（同 `ports/lua`；CL 版靠 `*load-truename*` 有解）。
  直接 `./hermy` 或整個資料夾在 PATH 都可跑；若把單一 CLI 以 symlink 掛到別處 bin 才會找不到庫。
- `run-skill` 用 `timeout 300` 包住技能（同 Python/Lua 版）。其餘行為與輸出格式一致。

## 相依

- `fennel`（PUC Lua 5.4）、`curl`、`jq`。
