# hermy（s7 Scheme 版）— curl+jq shell-out 移植

← [hermy（Python 原版）](../../README.md)｜[handy/AGENTS.md](../../../AGENTS.md)

[hermy](../../README.md)（用 handy 思想復刻 hermes-agent 的最小自我改進技能型 agent）的
**s7 Scheme 移植**，逐檔與 Python 原版 **1:1 對齊行為**。s7 沒有原生 JSON/HTTP，也**刻意不手刻**——
一律 shell-out 給現成的 `curl`＋`jq`，s7 只當 glue/loop。

## s7 怎麼跑（重要）

s7 **沒有獨立 PATH 執行檔**；透過 cllm 內嵌 s7＋binding 的 **`llm-s7`** 跑：`llm-s7 <script.scm> [args…]`。
先 `source ~/dev/cllm/env.sh`（`llm-s7` 在 `~/dev/bin/llm-s7`）。**本移植的每支 CLI 都是 shell shim**，
會自己備妥 `llm-s7` 再 exec，故直接 `./hermy …` 即可，不必手動 source。

已用到的 s7 能力：`*argv*`＝script 之後的參數字串 list（**不含 script 路徑**）；`(getenv "X")`；
**`(system cmd #t)` 回傳該命令 stdout 字串**（curl/jq 靠這個捕捉）；`(system "cat > f")` 可把
**本行程 stdin** 寫進檔（s7 的 Scheme 層預設 input port 沒接行程 stdin，故讀自身 stdin 一律經此）；
`call-with-output-file` 寫暫存檔。

## 本質映射（同原版）

| hermes 核心 | hermy 的 handy 化 |
|---|---|
| **Skills**（目錄＋腳本＋LLM 工具、`create_skill` 自產）| **folder-as-callable**：`skills/<name>/`＝`skill.json`＋`run`。`create_skill`＝AI 自我擴展 |
| **對話迴圈**（模型→工具分派→結果→迴圈）| DeepSeek function-calling 迴圈，工具＝shell-out 呼叫技能 `run` |
| **記憶**（對話後同步、檔案化）| `memory/log.ndjson` append-log |
| **LLM provider** | DeepSeek（`deepseek-chat`）|

## 與 Python 原版的差異（＝本移植的重點）

- **HTTP 靠 `curl`、JSON 靠 `jq`**：s7 不手刻 http/json，全部 shell-out（`(system cmd #t)` 捕捉 stdout）。
  對齊 handy「拿現成程式用腳本包」與 CL/Fennel 版一致。
  - `ds-chat` = `jq`（組 request body、抽 `.choices[0].message`）+ `curl`（POST，`-w '\n%{http_code}'` 取狀態碼）。
  - `skills-json` / `mem-append` / `hermy` 的 messages 增修全走 `jq`。
- **messages 陣列存成 JSON 字串**，用 `jq` 增修（`. + [$msg]`、`--argjson` 帶入）；大陣列（messages）一律
  走 **input 暫存檔**餵 jq（避開 Linux 單一 argv 128KB 上限），只把小值（單則 msg、tools）走 `--argjson`。
- **薄殼＋庫，但殼是 shell（不是 s7）**：s7 **拿不到自身 script 路徑**，無法讓 s7 薄殼自定位庫。
  故每支 CLI（`hermy`＋`lib/*`）是一支 **shell shim**（`readlink -f` 算自身目錄→export `HERMY_ROOT`→
  備妥 `llm-s7`→`exec llm-s7 $HERMY_ROOT/src/util.scm <main名> "$@"`）。這正是 handy 慣例（如 llme 的 `_exec`）。
  所有邏輯（各工具的 main 與 `run-capture`/`jq`/`env`/I/O 助手）都在 **`src/util.scm`**（單一庫，一支檔）；
  dispatch 靠 `(car *argv*)`＝main 名、`(cdr *argv*)`＝真參數，庫從 env `HERMY_ROOT` 拿專案根。
- **UTF-8 全程**：argv／檔名／stdin／stdout／子行程輸出皆不亂碼（已驗中文與 emoji）。
- **細節偏差**：`iso-now` 為秒級 UTC（原版 Python `isoformat()` 帶微秒）；`run-skill` 未套原版 300s
  `timeout`（技能本身可自管）。讀自身 stdin 走 `cat`（s7 預設 input port 不接行程 stdin）。行為與輸出格式其餘一致。

## 用法

```sh
export DEEPSEEK_API_KEY=sk-...
./hermy 用 shell 技能看看當前資料夾有哪些檔案
echo "把 37 度攝氏轉華氏" | ./hermy
```

（每支 CLI 會自己 `source ~/dev/cllm/env.sh`；若已 source 過則略過。）

## 結構（每檔可單獨 CLI 呼叫）

```
ports/s7/
├─ hermy                shell shim（6 行）→ orchestrate：agent 迴圈
├─ lib/                 shell shim（各 6 行），可單獨呼叫
│  ├─ skills-json       → main-skills-json  掃 skills/ → OpenAI tools JSON（「發現」）
│  ├─ ds-chat           → main-ds-chat      stdin{messages,tools} → curl 一次 DeepSeek → message（「腦」）
│  ├─ run-skill <name>  → main-run-skill    stdin=參數 → exec skills/<name>/run（「手」）
│  └─ mem-append        → main-mem-append   stdin=record → append memory/log.ndjson（「記憶」）
├─ src/util.scm         ★ 庫：所有邏輯 + shell-out/jq/env/I/O 助手 + main dispatch
├─ prompt/system.md     system prompt（資料）
├─ skills/              技能庫（種子 shell/、create_skill/ 沿用原版，run 可為任何語言）
└─ memory/             記憶（log.ndjson，runtime、不進版控）
```

> **設計**：s7 的 CLI 層保持越薄越好（shell shim），重活丟給 `curl`/`jq` 與庫。換掉某個 main 的行為只改
> `src/util.scm`、不動 shim。

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
| `HERMY_ROOT` | shim 自算 | 專案根（shell shim 傳給 s7 庫；s7 定位不到自身路徑）|

## 相依

- `llm-s7`（cllm 內嵌 s7；`source ~/dev/cllm/env.sh`）、`curl`、`jq`、`bash`（shim）。
- 技能 `run` 可用任何語言（種子 `shell`/`create_skill` 為 Python）。
