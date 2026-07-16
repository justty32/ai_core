# 踩坑 · 真後端 + 測試資料

← [gotchas 索引](README.md)｜[common/README](../README.md)｜[INDEX](../../../INDEX.md)

打通本機 LM Studio 首驗收踩到的坑（錯誤路徑／reasoning／schema），與測試資料的坑。**這些離線 `file://` fixture 驗不出**，是真後端的活。

## 真後端（打通 LM Studio 首驗收）

- **★★★ 方法論：「自己寫的離線 fixture」結構上就測不出「真後端長什麼樣」**。fixture 是「我們以為後端會回什麼」的投影，**只投影「成功」、從不投影「失敗」**——所以它只能驗「我們已經想到的東西」。同一病根連咬多次（schema 少欄位、後端 error JSON、reasoning 空答案）。**對策**：離線 fixture 該**刻意塞進真實世界的髒東西**（跳脫序列、缺欄位、多餘欄位、reasoning_content、**以及 error JSON／4xx 這種失敗回應**）；且**新接口一定要對真後端打一次**才算數（記到 [WAIT_USER](../../../WAIT_USER.md)）。
- **★★ 後端 error JSON 別靜默吞成空字串——`choices` 空就回 `{}` 是護欄破洞**（2026-07-15 真後端驗揪出）：OpenAI 相容後端出錯（LM Studio 模型載入失敗、雲端 401/429…）回 `{"error":{"message":…}}`＋HTTP 4xx、**無 `choices`**。若寫「解不動或 `choices` 空 → 回空」，後端錯誤就**被吞成空字串、還報成功**——對「笨模型＋護欄」這正是最該堵的洞。**對策**：解析前先攔——`guard_backend_error(status, raw)`（解 error 物件 throw、或 HTTP≥400 帶狀態碼＋body 片段 throw），非串流派送前攔一次、串流路徑接 `http::stream` 的 status＋累積 raw 再攔一次；`llm_ask` 把例外收斂成 `on_error`＋回 `LLM_ERROR`。**離線 `file://` 回 status=200 且無 error 物件，不受影響**。
- **★ reasoning 模型：`max_tokens` 是「reasoning＋content」一起算的——手動給小值會拿到空答案**：gemma-4-e4b 這類 reasoning 模型把思考鏈放 `message.reasoning_content`、答案才在 `message.content`，**共用同一份 `max_tokens` 預算**。手動給 `max_tokens=600` → 預算被 reasoning 吃光、`finish_reason: length`、`content` 空。症狀「後端明明有回、程式卻拿到空的」極易誤診成解析壞掉——看 `usage.completion_tokens_details.reasoning_tokens` 就知道花去哪。**⚠ 別過度反應**：取樣欄位全選填、沒設就不送＝**預設路徑本來就對**（交後端用 context 上限，實測 `finish_reason: stop`、content 完整）。這坑**只在你手動設了 `max_tokens` 才咬人**。
- **★★ schema 的 `required`：沒列進 `required` 的欄位＝選填，後端可以少給**：JSON Schema 語意下約束解碼只保證「不多給鍵」、**不保證「每個欄位都給」**——實測 LM Studio + gemma 對 `{name,affection,lines}` 的 schema **若沒標 required 只吐 `{"name":…}`**。C ABI 收的是**原始 schema JSON 字串**（`llm_schema_t.json`／`llm_tool_def_t.parameters`，caller 自備），故 `required` 要不要標、標哪些是**寫 schema 檔那方的責任**。**離線 fixture 驗不出**（假回應欄位當然齊）。（舊 L0 曾有 `schema_of<T>()` 用 `error_on_missing_keys` 自動生 required 的便利層，已封存進 `archived/llm_schema.hpp`；反射生成 schema 的便利層留待日後在 `cabi.hpp` 上層重長。）

## 測試資料

- **離線 fixture 別被 `.gitignore` 蓋掉＝測試在新 clone 上根本跑不了**：fixture 是**測試資料、不是 build 產物，該進版控**。本專案 `test/fixtures/` 一開始就做對（`cli_smoke.sh` 全走 `file://`、不連網、17/17）。
