# hermy（Common Lisp / SBCL 版）— curl+jq shell-out 移植

← [hermy（Python 原版）](../../README.md)｜[handy/AGENTS.md](../../../AGENTS.md)

[hermy](../../README.md)（用 handy 思想復刻 hermes-agent 的最小自我改進技能型 agent）的
**Common Lisp 移植**，逐檔與 Python 原版 **1:1 對齊行為**。純 **SBCL 2.6.6**（`#!/usr/bin/sbcl --script`，
只用內建 `sb-ext:*posix-argv*` / `posix-getenv` / `run-program`），**不引 quicklisp / dexador / shasht**。

## 本質映射（同原版）

| hermes 核心 | hermy 的 handy 化 |
|---|---|
| **Skills**（目錄＋腳本＋LLM 工具、`create_skill` 自產）| **folder-as-callable**：`skills/<name>/`＝`skill.json`＋`run`。`create_skill`＝AI 自我擴展 |
| **對話迴圈**（模型→工具分派→結果→迴圈）| DeepSeek function-calling 迴圈，工具＝shell-out 呼叫技能 `run` |
| **記憶**（對話後同步、檔案化）| `memory/log.ndjson` append-log |
| **LLM provider** | DeepSeek（`deepseek-chat`）|

## 與 Python 原版的差異（＝本移植的重點）

- **HTTP 靠 `curl`、JSON 靠 `jq`**：CL 不用 http/json 函式庫，全部 shell-out（`sb-ext:run-program`）給現成
  unix 引擎。對齊 handy「拿現成程式用腳本包」——每檔更小、免 quicklisp 載入延遲。
  - `ds-chat` = `jq`（組 request body、抽 `.choices[0].message`）+ `curl`（POST，`-w '%{http_code}'` 取狀態碼）。
  - `skills-json` / `mem-append` / `hermy` 的 messages 增修全走 `jq`。
- **messages 陣列存成 JSON 字串**，用 `jq` 增修（`. + [$msg]`、`--argjson` 帶入），大陣列一律走 **stdin**
  餵 jq（避開 Linux 單一 argv 128KB 上限）。
- **CLI 薄殼各 5 行、邏輯全在庫**：每個可執行檔（`hermy`＋`lib/*`）只做「載入 `src/util.lisp`＋
  呼叫一個 main」，**≤10 行**；所有邏輯（含各工具的 main 與 `run`/`jq`/`env`/I/O 助手）都在
  `src/util.lisp` 的 **`package hermy`**（195 行，不限）。庫**自己**從 `*load-truename*` 算出 `*root*`
  （**自動解 symlink**），故 CLI 不必設路徑。要重用就 `(load …/util.lisp)` 後 `(hermy:orchestrate)` 等。
- **UTF-8 全程**：`sb-impl::*default-external-format* = :utf-8`、`run-program :external-format :utf-8`，
  argv／檔名／stdin／stdout／子行程輸出皆不亂碼（已驗中文與 emoji）。
- **細節偏差**：`iso-now` 為秒級 UTC（原版 Python `isoformat()` 帶微秒）；`run-skill` 未套原版的
  300s `timeout`（SBCL `run-program` 無內建 timeout 參數）——技能本身可自管。行為與輸出格式其餘一致。

## 用法

```sh
export DEEPSEEK_API_KEY=sk-...
./hermy 用 shell 技能看看當前資料夾有哪些檔案
echo "把 37 度攝氏轉華氏" | ./hermy
```

## 結構（每檔可單獨 CLI 呼叫）

```
ports/cl/
├─ hermy                CLI 薄殼（5 行）→ (hermy:orchestrate)：agent 迴圈
├─ lib/                 CLI 薄殼（各 5 行），可單獨呼叫
│  ├─ skills-json       → (hermy:skills-json)  掃 skills/ → OpenAI tools JSON（「發現」）
│  ├─ ds-chat           → (hermy:ds-chat)      stdin{messages,tools} → curl 一次 DeepSeek → message（「腦」）
│  ├─ run-skill <name>  → (hermy:run-skill)    stdin=參數 → exec skills/<name>/run（「手」）
│  └─ mem-append        → (hermy:mem-append)   stdin=record → append memory/log.ndjson（「記憶」）
├─ src/util.lisp        ★ 庫（package hermy，195 行）：所有邏輯 + shell-out/jq/env/I/O 助手
├─ prompt/system.md     system prompt（資料）
├─ skills/              技能庫（種子 shell/、create_skill/ 沿用原版，run 可為任何語言）
└─ memory/             記憶（log.ndjson，runtime、不進版控）
```

> **設計**：CLI 是純入口（≤10 行），邏輯集中在庫、需要才 `(load)`。這正是把「可重用的東西抽出來、
> 有需要再 import」推到極致——換掉某個 main 的行為只改庫、不動 CLI。

單獨測（免 API）：`./lib/skills-json`、`echo '{"command":"ls"}' | ./lib/run-skill shell`。

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

## 相依

- SBCL（`/usr/bin/sbcl --script`）、`curl`、`jq`。
