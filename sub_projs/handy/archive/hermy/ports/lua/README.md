# hermy — Lua 移植版（curl + jq，無 cjson）

← [hermy（Python 真相源）](../../README.md)｜[handy/AGENTS.md](../../../AGENTS.md)

hermy（自我改進技能型 agent）的 **Lua 移植版**，行為與 Python 版 **1:1 對齊**。差別只在實作載體：Lua 無 JSON/HTTP 內建，於是**所有 JSON 操作 shell-out 給 `jq`、HTTP 給 `curl`**——正合 handy「拿現成 unix 引擎用腳本包」的精神。

## 本質映射（與 Python 版相同）

| hermes 核心 | hermy 的 handy 化 |
|---|---|
| **Skills**（`create_skill` 自產） | **folder-as-callable**：`skills/<name>/`＝`skill.json`＋`run`；`create_skill`＝AI 自我擴展 |
| **對話迴圈** | DeepSeek function-calling 迴圈，工具＝shell-out 呼叫技能 `run` |
| **記憶** | `memory/log.ndjson` append-log |
| **LLM provider** | DeepSeek（`deepseek-chat`）|

「腦」＝DeepSeek（`ds-chat` 直打 `/v1/chat/completions`），「手」＝技能資料夾（shell-out）。

## 用法

```sh
export DEEPSEEK_API_KEY=sk-...
./hermy 用 shell 技能看看當前資料夾有哪些檔案
echo "把 37 度攝氏轉華氏" | ./hermy
```

## 結構（每個 lib/* 都能單獨 CLI 呼叫）

```
ports/lua/
├─ hermy                編排器：把 lib/* 用 CLI 串成 agent 迴圈；messages 存 JSON 字串、用 jq 增修
├─ lib/                 可單獨呼叫的小原語
│  ├─ skills-json       掃 skills/ → jq 併成 OpenAI tools JSON 陣列（「發現」）
│  ├─ ds-chat           stdin{messages,tools} → jq 組 body → curl DeepSeek → stdout message（「腦」）
│  ├─ run-skill <name>  stdin=參數 → exec skills/<name>/run → stdout 結果（「手」）
│  └─ mem-append        stdin=record → jq 加 ts → append memory/log.ndjson（「記憶」）
├─ prompt/system.md     system prompt（原樣照抄 Python 版）
├─ skills/              技能庫（原樣照抄種子技能）
│  ├─ shell/            skill.json + run（run 為 python3；folder-as-callable 不限語言）
│  └─ create_skill/     自我擴展：寫出新的 skills/<name>/
└─ memory/              記憶（log.ndjson，runtime、不進版控）
```

單獨測範例：`./lib/skills-json`；`echo '{"command":"ls"}' | ./lib/run-skill shell`。

## 介面契約（與 Python 版逐一對齊）

- `ds-chat`：stdin `{messages,tools}` → stdout message JSON。env `HERMY_ENDPOINT`／`HERMY_MODEL`／`DEEPSEEK_API_KEY`。
- `skills-json`：掃 `../skills/*/skill.json` → OpenAI tools JSON 陣列（`_` 開頭資料夾忽略、需同時有 `skill.json`＋`run`）。
- `run-skill <name>`：stdin=參數 → exec `../skills/<name>/run` → 結果（截斷 8000、退出碼標註）。
- `mem-append`：stdin=record → 加 `ts` → append `../memory/log.ndjson`。
- `hermy`：`skills-json` → 迴圈{`ds-chat` → tool_calls 逐個 `run-skill`、`create_skill` 後重載技能 → 回饋} → 答；全程 `mem-append`；system 讀 `prompt/system.md`；`HERMY_MAX_STEPS` 預設 12。

## 環境變數

| 變數 | 預設 | 作用 |
|--|--|--|
| `DEEPSEEK_API_KEY` | （必給）| DeepSeek 金鑰 |
| `HERMY_MODEL` | `deepseek-chat` | 模型 |
| `HERMY_ENDPOINT` | `https://api.deepseek.com/v1/chat/completions` | endpoint |
| `HERMY_MAX_STEPS` | `12` | 工具迴圈上限 |
| `HERMY_SKILLS_DIR` / `HERMY_MEMORY_DIR` | 同層 `skills/`／`memory/` | 覆寫目錄 |

## 與 Python 版的差異

- **JSON**：Python 版用 `json` 模組；此版所有 JSON 組/解都 shell-out 給 **`jq`**（messages 陣列存成 JSON 字串，增修用 `jq -c -s '.[0] + [.[1]]'` 等）。
- **HTTP**：Python 版用 `urllib`；此版 `ds-chat` 用 **`curl`**（body 由 `jq` 組、`-d @file` 送出）。
- **無 cjson**：本機 Lua 無 cjson，故刻意不依賴任何 Lua 第三方庫——只靠 `lua` + `jq` + `curl` + `timeout`。
- **shell 轉義**：中文/空白/單引號一律經自寫 `q()`（`'`→`'\''`）＋暫存檔餵 stdin，安全。
- **直譯器**：`#!/usr/bin/env lua`（本機為 Lua 5.5；luajit / lua5.4 亦可，未用任何版本相依語法）。
- 種子技能 `run` 仍是 Python（folder-as-callable 對語言中立，原樣照抄以保 1:1）。
