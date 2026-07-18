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

## 結構（folder-as-callable）

```
hermy/
├─ hermy                主執行檔（Python：技能發現 + DeepSeek function-calling 迴圈 + 記憶）
├─ skills/              技能庫（每個子資料夾＝一個工具）
│  ├─ shell/            範例技能「手」：skill.json + run（sh -c 執行命令）
│  └─ create_skill/     自我擴展：skill.json + run（寫出新的 skills/<name>/）
└─ memory/             記憶（log.ndjson，runtime、不進版控）
```

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

分析來源：`~/repo/pas/analysis/hermes-agent/`（L1–L6＋SDK 深剖）；原始碼：`~/repo/pas/projects/hermes-agent/`。
