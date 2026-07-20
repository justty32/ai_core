# 各後端「結構化輸出」能力矩陣

← [docs 索引](README.md)｜[gotchas/backend](../workflows/common/gotchas/backend.md)

**問題**：cllm 對「OpenAI 相容」後端送結構化輸出請求。實測 DeepSeek 拒收 OpenAI 的 `json_schema` response_format（HTTP 400），改用 function calling 才拿到結構化 args。**這通用嗎？** 下表是跨後端的調查。

> **資料性質**：除 **DeepSeek 為我們實測**（2026-07-20，Linux `~/dev/bin/llm`）外，其餘皆**官方文檔依據**（2026-07-20 由 subagent 查證，非實測）。能力隨版本變，接新後端仍應**真打一次**（[gotchas/backend 方法論](../workflows/common/gotchas/backend.md)）。

## 三套機制

1. **`response_format: {type:"json_schema", strict}`** — OpenAI 2024「Structured Outputs」，文法級嚴格約束。cllm 的 `--schema` 送這個。
2. **`response_format: {type:"json_object"}`** — 舊「JSON mode」，只保證合法 JSON、不約束 schema。**cllm 目前無此旗標**。常見前置要求：prompt 須含 "json" 字樣。
3. **function/tool calling** — 工具 `parameters`＝JSON schema，模型回 tool_call 的 `arguments`。cllm 的 `--tool` 送這個。

## 矩陣

標記：`✅`支援／`❌`不支援／`⚠️`部分（限模型／有條件）／`❓`文檔不明。

| 後端 | ① json_schema strict | ② json_object | ③ tool calling（嚴格？） | 這家推薦的結構化路 |
|---|---|---|---|---|
| **OpenAI** | ✅ gpt-4o-2024-08+／4.1／5 | ✅（須 "json"）| ⚠️ `strict` 逐工具選填 | json_schema `strict:true` |
| **Azure OpenAI** | ✅ API≥2024-08、綁部署版本 | ✅ | ✅ strict（**須關 `parallel_tool_calls`**）| json_schema（走 `v1` 端點）|
| **DeepSeek** 〔實測〕| ❌ 只 text/json_object | ✅（須 "json"＋範例）| ✅ 非 strict；strict 要 **beta URL**＋有未修壞 JSON bug | **tool calling**（＝我們走的路）|
| **Gemini**（OpenAI 相容層）| ⚠️ 即時可、**Batch 失敗** | ✅ | ✅（`tool_choice` 細節不明）| 即時 json_schema／batch 退 json_object |
| **Gemini**（原生）| ✅ `responseSchema`（保語法非語義）| ✅ | ✅ 模式 AUTO／ANY／VALIDATED | responseSchema 或 mode=ANY |
| **Anthropic**〔走 proxy〕| ✅ Structured Outputs（`output_config.format`）| ❌ 無 schema-less 級 | ✅ strict tool use（`strict:true`＋`tool_choice:any`）| Structured Outputs 或 strict tool |
| **Mistral** | ✅ 除 `codestral-mamba` | ✅ | ⚠️ `strict` 欄位未文檔化 | json_schema `strict:true` |
| **Groq** | ⚠️ strict 限 gpt-oss-20b/120b；**與 stream/tool 互斥** | ✅ | ⚠️ 無 strict | GPT-OSS→strict；其餘→tool／json_object |
| **Together** | ⚠️/❓ 無明確 strict 保證 | ❓（JSON mode 存在、文檔淡）| ✅ `tool_choice` 全 | tool calling |
| **Fireworks** | ⚠️ 無 strict 旗標但自動 `unevaluatedProperties:false`；**與 reasoning 互斥** | ✅（沒指示會無限空白輸出）| ✅ `tool_choice` 全（含 `any`）| json_schema（不需 reasoning 時）|
| **OpenRouter**（聚合器）| ⚠️ **透傳**底層；`require_parameters:true` 才過濾、否則靜默丟參數 | ✅（須 "json"）| ✅ best-effort | `require_parameters`＋json_schema strict |
| **llama.cpp** server | ✅ json_schema→GBNF | ✅ | ⚠️ 需 `--jinja` | json_schema（GBNF 強制）|
| **vLLM** | ✅ guided decoding／xgrammar | ❓ | ✅ `tool_choice=required` 嚴格 | json_schema 或 tool_choice=required |
| **Ollama** | ✅ v0.5+ `format`＝schema→GBNF | ✅（須指示 json）| ⚠️ grammar 強制未補齊 | `format`＝JSON Schema |
| **LM Studio** | ✅ ≥7B 模型、GGUF grammar | ❓ | ⚠️ 無具名 `tool_choice` | json_schema |

## 怎麼讀這張表（回答「通用嗎」）

- **① json_schema 被拒＝DeepSeek 是相對少數**，不是通例：所有**第一方雲端**（OpenAI/Azure/Mistral/Anthropic/Gemini 原生）與**所有本機執行時**（llama.cpp/vLLM/Ollama/LM Studio，靠 grammar/GBNF）都支援。**真正不支援/看情況的**是 DeepSeek、部分替代雲（Groq 限兩模型、Together/Fireworks 無明確 strict）、以及聚合器（Gemini 相容層 batch、OpenRouter 透傳）。
- **③ tool calling ＝三者中最通用**：15 個後端全支援——所以「改用 `--tool`」作為**可攜策略**是站得住的。**但嚴格保證不均**：只有 OpenAI/Azure/Anthropic/vLLM 有明確 strict；DeepSeek/Mistral/Groq/Together/Ollama/LM Studio 多為 best-effort（可能給錯型別/漏欄位）。
- **② json_object ＝廣泛但有兩個通用坑**：多數要 prompt 含 "json"／明確指示（不然 DeepSeek/Fireworks 會無限空白輸出）；**Anthropic 沒有這一級**（直接跳到帶 schema）。
- **一條反覆出現的規律**：「OpenAI 相容」只保證 wire format 像，**不保證能力面**。不支援時的行為還分兩種——**硬失敗**（DeepSeek 回 400，好偵測）vs **靜默丟棄**（OpenRouter 無 `require_parameters`、Gemini 相容層非清單參數），後者最危險（你以為生效了其實沒有）。

## 對 cllm 的意涵

- **別把可靠性押在後端 strict schema 上**（太脆、不通用）。最通用的組合＝**tool calling ＋ client 端 schema 驗證 ＋ retry/guard**（呼應 roadmap 的「行數助手＋retry/guard」）。
- **cllm 缺一條 `json_object` 路**：對只支援 json_object 的後端（DeepSeek 純 JSON 需求、Together…），目前只能純 prompt 要 JSON。若要補，加個 `--json` 送 `{type:"json_object"}` 即可（但 tool calling 已能拿更強約束，優先序低）。
- **偵測後端能力**：靜默丟棄的後端（OpenRouter/Gemini 相容層）驗不出「有沒有生效」，只能看回應**實際**有沒有符合 schema——不能看有沒有報錯（同我們對 LM Studio `modalities` 的實測教訓）。

## ⚠️ 時效性（2026-07-20 查到、值得盡快確認）

- **DeepSeek 模型改名**：`deepseek-chat`／`deepseek-reasoner` 預告 **2026-07-24 UTC 棄用** → 對應到 `deepseek-v4-flash` 的非思考／思考模式。且舊文檔載 `deepseek-reasoner`（思考模式）**不支援 function calling**——若之後改用新模型名，我們拿結構化輸出的 `--tool` 路要重新確認可用性。
- 各家 strict/模型清單都在動（Groq 的 GPT-OSS 限制、Anthropic 的 `output_format`→`output_config` 遷移、vLLM `guided_json`→`structured_outputs` 遷移），引用請看各段 as-of 日期。

## 出處

各後端官方文檔（api-docs.deepseek.com／developers.openai.com／learn.microsoft.com／platform.claude.com／docs.mistral.ai／ai.google.dev／openrouter.ai/docs／console.groq.com／docs.together.ai／docs.fireworks.ai／llama.cpp・vLLM・Ollama・LM Studio 官方文檔），2026-07-20 查證。DeepSeek 一列另有本專案實測佐證。
