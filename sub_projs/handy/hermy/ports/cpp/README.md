# hermy (C++ port) — 用 curl+jq 當引擎的最小自我改進技能型 agent

← [hermy/](../../README.md)｜[handy/AGENTS.md](../../../AGENTS.md)

[hermy](../../README.md)（Python 版）的 **C++ 1:1 移植**。行為與介面契約逐一對齊原版；差別只在**實作載體**：C++ 不引 HTTP/JSON 函式庫，**HTTP 全部 shell-out 給 `curl`、JSON 全部 shell-out 給 `jq`**（現成 unix 引擎）。這樣每個檔仍能保持小、且對齊 handy「拿現成程式用腳本包」的精神。

## 本質映射（hermes → handy → 本 C++ port）

| hermes 核心 | hermy 的 handy 化 | C++ port 的做法 |
|---|---|---|
| **Skills**（工具、`create_skill` 自產）| **folder-as-callable**：`skills/<name>/`＝`skill.json`＋`run` | 種子技能原樣照抄（語言中立）；`skills-json` 用 `jq` 讀 `skill.json` |
| **對話迴圈**（模型→工具分派→結果→迴圈）| DeepSeek function-calling 迴圈 | `hermy` 用 `jq` 自管 messages 陣列；`ds-chat` 用 `curl` 打 API |
| **記憶** | `memory/log.ndjson` append-log | `mem-append` 用 `jq` 加 ts 後 append |
| **LLM provider** | DeepSeek（`deepseek-chat`）| 同 |

## 與 Python 版的差異（唯一本質差別＝改用 curl+jq）

| 面向 | Python 版 | C++ port |
|---|---|---|
| HTTP | `urllib.request` | `curl`（shell-out） |
| JSON 解析/建構 | `json` 模組 | `jq`（shell-out，每次操作一次） |
| 子行程/stdin 餵入 | `subprocess.run` | `fork`/`execvp` + pipes（`src/proc.hpp`） |
| `run-skill` 硬逾時 | `timeout=300` | 未實作（技能 `run` 自身可設；`shell` 技能內部仍 120s） |
| `skills-json` 輸出空白 | `json.dumps` 有 `, `/`: ` 空格 | `jq -c` 緊湊（語意相同，下游都再經 jq/解析） |
| `[:8000]`/`[:2000]` 截斷 | 依 Unicode 字元 | 依位元組（UTF-8 邊界極端情況可能切字，僅影響截斷處） |

其餘（環境變數、system prompt、迴圈語意、記憶格式、技能契約）與原版一致。

## 依賴

`curl`、`jq` 需在 PATH（本機驗於 curl 8.21、jq 1.8.2）。編譯器 g++16 或 clang++22（C++20）。

## 建置

```sh
./build.sh                 # 預設 g++；CXX=clang++ ./build.sh 亦可
```

編出 `./hermy`（根）與 `./lib/{skills-json,ds-chat,run-skill,mem-append}`（同層 `lib/` 下，維持與原版相同相對路徑）。

## 用法

```sh
export DEEPSEEK_API_KEY=sk-...
./hermy 用 shell 技能看看當前資料夾有哪些檔案
echo "把 37 度攝氏轉華氏" | ./hermy
```

各原語皆可單獨 CLI 呼叫（unix-composition）：

```sh
./lib/skills-json                                  # 印 OpenAI tools JSON 陣列
echo '{"command":"echo hi"}' | ./lib/run-skill shell   # → hi
echo '{"messages":[...],"tools":[...]}' | ./lib/ds-chat # 一次 DeepSeek 呼叫
echo '{"role":"user","content":"x"}' | ./lib/mem-append # append memory/log.ndjson
```

## 結構

```
ports/cpp/
├─ hermy                編排器（binary）：把 lib/* 用 CLI 串成 agent 迴圈；messages 用 jq 自管
├─ lib/                 可單獨呼叫的小原語（binary）
│  ├─ skills-json       掃 skills/ → jq → 印 OpenAI tools JSON（「發現」）
│  ├─ ds-chat           stdin{messages,tools} → curl DeepSeek → stdout message（「腦」）
│  ├─ run-skill <name>  stdin=參數 → exec skills/<name>/run → stdout 結果（「手」）
│  └─ mem-append        stdin=一筆 record → jq 加 ts → append memory/log.ndjson（「記憶」）
├─ src/                 C++ 原始碼（proc.hpp 共用 shell-out/時間/字串小工具）
├─ build.sh             編譯腳本
├─ prompt/system.md     system prompt（資料，照抄原版）
├─ skills/              技能庫（種子照抄；folder-as-callable 語言中立）
│  ├─ shell/            範例技能「手」
│  └─ create_skill/     自我擴展：寫出新的 skills/<name>/
└─ memory/              記憶（log.ndjson，runtime、不進版控）
```

## 環境變數（同原版）

| 變數 | 預設 | 作用 |
|--|--|--|
| `DEEPSEEK_API_KEY` | （必給）| DeepSeek 金鑰 |
| `HERMY_MODEL` | `deepseek-chat` | 模型 |
| `HERMY_ENDPOINT` | `https://api.deepseek.com/v1/chat/completions` | endpoint |
| `HERMY_MAX_STEPS` | `12` | 工具迴圈上限 |
| `HERMY_SKILLS_DIR` / `HERMY_MEMORY_DIR` | 同層 `skills/`／`memory/` | 覆寫目錄 |
