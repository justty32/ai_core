# hermy — 用 handy 思想復刻 hermes-agent（最小自我改進技能型 agent）

← [handy/AGENTS.md](../AGENTS.md)｜[INDEX](../INDEX.md)

[hermes-agent](https://github.com/NousResearch/hermes-agent)（Nous Research 的「自我改進型 AI Agent」）的**最小骨架復刻**，用 handy 思想重寫、後端 **DeepSeek**。不是抄它 3000 個 py 檔，而是抓它的**本質**用 handy 的方式落地。

## 本質映射（hermes → handy）

| hermes 核心 | hermy 的 handy 化 |
|---|---|
| **Skills**（`skills/` 目錄、`SKILL.md`＋腳本、包成 LLM 工具、Agent 能 `create_skill` 自產）| **folder-as-callable**：`skills/<name>/`＝`skill.json`（工具宣告）＋`run`（執行體）。`create_skill` 讓 agent 自寫新技能＝**AI 自我擴展** |
| **對話迴圈**（模型→工具分派→結果→迴圈）| DeepSeek function-calling 迴圈，工具＝shell-out 呼叫技能 `run` |
| **記憶**（對話後同步、檔案化）| `memory/log.ndjson` append-log |
| **LLM provider** | DeepSeek（`deepseek-chat`）|
| Curator（閒置自我維護技能）| 〔待做〕定期檢視/合併/修技能，對接 handy tick |

> **「腦」＝DeepSeek**（直打 `/v1/chat/completions`，本迴圈自管 messages 陣列才能多輪帶 tool 結果——cllm 的 `llm` CLI 是單發、餵不回 tool-role，故這裡直接打 API；仍是 handy 精神：LLM 只是驅動核心，真正的事由技能做）。**「手」＝技能資料夾**（shell-out）。

## 用法

```sh
export DEEPSEEK_API_KEY=sk-...
./hermy 用 shell 技能看看當前資料夾有哪些檔案
echo "把 37 度攝氏轉華氏" | ./hermy
```

agent 會：發現技能 → 組工具給 DeepSeek → 模型決定呼叫哪個技能 → shell-out 執行 → 結果回饋 → 迴圈直到給出答案。缺技能時自己 `create_skill` 寫一個再用。

## 結構（folder-as-callable ＋ CLI 串連的小檔）

刻意拆到**每檔 <50 行、每個都能單獨 CLI 呼叫**，`hermy` 只是把它們用 CLI 串起來的薄編排器（unix-composition）：

```
hermy/
├─ hermy                編排器（<50 行）：把 lib/* 用 CLI 串成 agent 迴圈；messages 自管
├─ lib/                 可單獨呼叫的小原語（每個 <50 行）
│  ├─ skills-json       掃 skills/ → 印 OpenAI tools JSON（「發現」）
│  ├─ ds-chat           stdin{messages,tools} → 一次 DeepSeek 呼叫 → stdout message（「腦」）
│  ├─ run-skill <name>  stdin=參數 → exec skills/<name>/run → stdout 結果（「手」）
│  └─ mem-append        stdin=一筆 record → append memory/log.ndjson（「記憶」）
├─ prompt/system.md     system prompt（資料，不是碼）
├─ skills/              技能庫（每個子資料夾＝一個工具）
│  ├─ shell/            範例技能「手」：skill.json + run（sh -c 執行命令）
│  └─ create_skill/     自我擴展：skill.json + run（寫出新的 skills/<name>/）
└─ memory/             記憶（log.ndjson，runtime、不進版控）
```

每個 `lib/*` 都能自己跑、自己測，例如 `./lib/skills-json`、`echo '{"command":"ls"}' | ./lib/run-skill shell`。`hermy` 迴圈＝`skills-json` → 迴圈{ `ds-chat` → 若有 tool_calls 逐個 `run-skill` → 回饋 } → `mem-append`。

**技能契約**：`skills/<name>/skill.json`＝`{"name","description","parameters":<JSON schema>}`；`skills/<name>/run`＝可執行，**從 stdin 收 JSON 參數、印結果於 stdout**。`_` 開頭資料夾忽略。agent 用 `create_skill` 產出的新技能下一步即可呼叫。

## 為何 Python（而非 handy 主力 Fennel）

agent loop 有真邏輯＋JSON 解析（tool_calls）＋HTTP，且**緊接 LLM 呼叫**（冷啟 ~50ms 埋在 token 延遲裡＝雜訊，同 llme 的理由）。Python stdlib（urllib/json/subprocess）最順手、且對齊 hermes 本身。技能 `run` 可用任何語言（folder-as-callable 不限）。

## 環境變數

| 變數 | 預設 | 作用 |
|--|--|--|
| `DEEPSEEK_API_KEY` | （必給）| DeepSeek 金鑰 |
| `HERMY_MODEL` | `deepseek-chat` | 模型 |
| `HERMY_ENDPOINT` | `https://api.deepseek.com/v1/chat/completions` | endpoint |
| `HERMY_MAX_STEPS` | `12` | 工具迴圈上限 |
| `HERMY_SKILLS_DIR` / `HERMY_MEMORY_DIR` | 同層 `skills/`／`memory/` | 覆寫目錄 |

## 待長（對齊 hermes 尚未做的本質）

- **Curator 自我維護**：閒置定期檢視技能（活躍/陳舊/封存、合併、出 patch），對接 handy `tick`/`routines`。
- **記憶預取＋壓縮**：對話前依任務 grep `log.ndjson` 注入相關記憶（context fencing `<memory-context>`）；超長時 LLM 迭代摘要輪轉。
- **部分求值**：技能路由/常見判斷能規則化的先固定成腳本（照 handy [new-resident](../workflows/new-resident.md) 原則），少花 LLM call。
- **權限/守衛**：`shell` 技能目前無限制動手；之後加 pre-tool 核准或 allowlist（對齊 hermes hooks）。

## 多語言移植（`ports/`）

同一套拆解在其他語言各實作一版，放 `ports/{cpp,lua,cl}/`（Python 原版即本目錄）。**設計統一：HTTP 靠 `curl`、JSON 靠 `jq`（現成 unix 引擎），語言只當 glue/loop**——這樣無原生 JSON 的語言也能保持小檔、且對齊 handy「拿現成用腳本包」。

| 版本 | 跑法 | 備註 |
|---|---|---|
| （本目錄）Python | `./hermy …` | 原版，用 stdlib `json`/`urllib`；每檔 <50 行 |
| `ports/cpp/` | `./build.sh` 後 `./hermy …` | g++/clang++；`lib/*` 全 <50，`hermy.cpp`(74)/`proc.hpp`(68) 過線 |
| `ports/lua/` | `./hermy …` | Lua（無 cjson）；`lib/*` 全 <50，`hermy`(76) 過線 |
| `ports/cl/` | `./hermy …` | SBCL `--script` 純內建、中文不亂碼；`lib/*` 全 <50，`hermy`(57)/`util.lisp`(57) 過線 |

三版都用真 DeepSeek 驗過工具迴圈（`3^4=81`）＋自我擴展（自寫 `rev` 技能反轉 handy→ydnah）。**共同取捨**：leaf 原語（skills-json/ds-chat/run-skill/mem-append）在各語言都 <50；只有編排器（＋C++/CL 的共用 helper）因 curl+jq 每步 shell-out 而略過 50——Python 靠 stdlib 免了這段。

分析來源：`~/repo/pas/analysis/hermes-agent/`（L1–L6＋SDK 深剖）；原始碼：`~/repo/pas/projects/hermes-agent/`。
