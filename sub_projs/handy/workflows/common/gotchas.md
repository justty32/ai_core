# 共通踩坑（跨住戶）

← [INDEX](../../INDEX.md)

不專屬任一住戶、任何人都可能撞到的坑，記/查這裡。某住戶專屬的坑記在該住戶自己的檔（如 `llme/README.md`、`zhtw` 檔頭）。

## 哪類坑記哪裡

| 坑的性質 | 記/查這裡 |
|---------|----------|
| 不專屬任一住戶的共通坑 | **common/gotchas**（本檔）|

---

- **glaze config 嚴格拒未知鍵**：cllm 的 `--config` 用 glaze 直接讀進 `Client` struct，**檔裡出現非 Client 欄位（如 `_notes`）→ `unknown_key` 解析失敗、exit 1**。真 config 檔（`llme/configs/<ep>.json`）只能有 `endpoint`/`model`/`api_key`/`timeout_ms` 等實欄位；`_template.json` 能放 `_notes` **只因模板從不被 `llm` 載入**。註解寫進 README，別寫進會被載入的 config。

- **DeepSeek 不支援嚴格 `--schema`（json_schema）**：`llm --config configs/deepseek.json --schema …` 會回 **HTTP 400「This response_format type is unavailable now」**。DeepSeek 服務端目前只支援 `response_format: json_object`、不吃 cllm 送的嚴格 `json_schema`（**DeepSeek 端限制、非 cllm bug**）。要結構化輸出：改用 `json_object` 提示，或用 `--tool` function-calling 繞（tool_calls 帶結構化 arguments，已驗可行）。

- **reasoning 模型的思考鏈吃 `--max-tokens` 預算**：會 `<think>` 的模型（qwen3.5、gemma、`deepseek-reasoner`…）**思考鏈與最終答案共用 `--max-tokens`**，給小值（如 1024）會偶爾被思考吃光 → `content` 空輸出。對策：給夠（`zhtw` 翻譯設 4096），或**reasoning 模型乾脆不設** `--max-tokens`（`llm --help` 也這樣建議）。

- **（已解，留脈絡）`llm` CLI 原本沒有 `--system`**：2026-07-18 前 zhtw 只能把人格折進 user prompt 開頭。已在 cllm 補上**真 system-role**（`--system <文字>` → cabi 在 user 訊息前插 `{"role":"system"}`；`LLM_DUMP_BODY=1` 可觀測送出 body，cli_smoke 35/35）。現在薄包裝一律用 `--system` 帶人格，llme 原樣轉發。
