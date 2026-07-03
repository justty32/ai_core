# Resume 指標

claude --resume 0fa7693b-d161-4a2c-9d48-625292ae50f2

> 注意：使用者跨電腦工作，Claude 的本地 memory 不跟隨；**repo 內的檔案（本檔 + ideas/notes + CLAUDE.md）才是唯一可靠的持久層**。重要脈絡一律落 repo。

## 最近里程碑（2026-07-03）：近期焦點①②落地——entry `format`/`schema` + 非軸欄位 `reliability`

§12.1 的兩個近期焦點已進正式核心與規範（tests 84 → **101 passed** 全綠）：

- **①輸出格式**：entry 新增 `format`（`"json"`/`"ndjson"` 或 `{"type":...}` 逃生口）與 `schema`（JSON Schema dict，ndjson 逐行適用）；非文字內容判定為**既有 `type: {"base":"binary","mime":...}` 已涵蓋**，不新增欄位。schema 同時定位為日後 deopt guard 的原料（§13.2）。
- **②可靠度**：頂層新增 **`reliability`（非軸欄位）**——number 0.0–1.0 或 `{value, measured_on, n, ...}` 帶 provenance；與第九軸分工＝證書 `stability` 凍結值 vs `reliability` 活值。
- 落地處：`src/ai_core/_core.py`、`tests/test_core.py`、`lib_spec.md`（新增 format/schema 小節 + 「非軸欄位：reliability」節）、`axis_spec.md §9` 交叉引用、`CLAUDE.md` 同步。
- 設計細節（format 值域、schema 掛 entry、reliability 形狀）＝Claude 拍板**待追認**，見 `try_implement/DECISIONS.md`「✅ 已收斂（2026-07-03）」。

**下一步**：近期焦點③＝entry manager 的**呼叫 trace log**（§13.2.4 已升格為承重件：每次 LLM 呼叫的 input/output/model 落 NDJSON——同時是 memoize 的表、測試組的原料、reliability 的計算來源）。原型落點：`try_implement/tools/llm_entry_manager.py` ＋ `lib/`。可參考 ATP v0 已凍結的 trace 慣例（`AI_CORE_TRACE_ID` + NDJSON，見 DECISIONS 2026-06-21 節）。

## 前一里程碑（2026-07-02）：回到初心——方向定調 + 範式論述 + prior-art 查證 + 逐點決斷

全文：`ideas/notes/20260702-2003-回到初心-llm-as-function.md`（§一～§十二，單一 commit `2592d33`）。

**定調**：回頭做最初需求——LLM as function + shell 處理 IO；規範層語言中立（Lisp 理想載體、Python 參考實作）；把 AI 當工具去馴化。

**範式**（§七～§十）：統一接口貫穿 function/shell/API 三種呼叫，差異外掛到 `--meta` 自我描述（類 Lisp list element）；LLM 降格為眾工具之一；範式兩缺點（抽象耗效能 + metadata 稅）只在 LLM 語境被中和 → 非通用範式、鎖死 scope。

**Prior-art 掃描**（§9.1，deep-research 查證）：四合一＝既有零件的新組合，無單一命名範式提出過；前三支柱已被 MCP / ToolRegistry / Lisp metaprogramming 論文做掉；**唯一逆主流＝第四支柱「LLM 降格」＝固化飛輪的結構性前提**。白地＝「LLM-as-peer → 熵減系統」。

**使用者逐點決斷**（§十二，下次動工依此）：
- **近期焦點**：①meta 補「輸出 JSON schema + 非文字內容格式」（解接縫問題）②可靠度先做成 meta 一個可變數值欄位 ③前期以「更好地使用 LLM」為主
- **已定案**：meta＝JSON、低限制、LLM 與確定性程式都讀；固化三機制（memoize/distill/codegen）全包
- **核心宗旨**：`ai_forge` 鑄刀——從 LLM 提取短邏輯鏈（＝Futamura 部分求值），固化是核心但很後面
- **既定方向**：成本×穩定度排程拍賣、雙向棘輪 de-固化、拓撲即資產、憑證 SBOM、可靠度預算、負空間認證、呼叫鏈歷史
- **押後**：憑證腐壞機制、測試組來源（心裡底稿＝Sutton 式 RL/探索）、燃料窗口

**下一步候選**：把「近期焦點 ①②」落進規範——meta 的 I/O 格式欄位設計（`core_nature/axis_spec.md` 或 data_format.md）+ 可靠度數值欄位；第三塊＝entry manager 的**呼叫 trace log**（見 §十三：整套系統＝語意的 tracing JIT——LLM=語意基準、固化三機制=tier 階梯、guard+deopt=de-固化、trace=測試組原料；輸出 schema 與可靠度數值正是 JIT 開工的頭兩塊地基）。

## 前一里程碑（2026-06-27）：core_handy defs 重新思考 + impl batch 切片

defs 層九軸收斂落地 `core_handy/defs/axes.hpp`；impl 層 batch-greenfield 垂直切片（io/intercept/state/cli/transaction）；`core_handy/main.cpp` 第一支回應 `--metadata` 的真 ac_helper；兩條 build 鏈皆綠。餘下 serve/stream/rate-meter/馴化框架缺消費者，依「停止鍵」收。
詳見 `core_handy/notes/impl_overview.md`。

**接續閱讀順序**：`roadmap.md` → `ideas/notes/20260702-2003-回到初心-llm-as-function.md`（最新方向）→ `core_handy/notes/00_index.md`（九軸定案）→ `try_implement/DECISIONS.md`（懸案清單）。
