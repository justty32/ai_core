# 02 · 標準函式集（stdlib）

> 定案＝基礎函數該有哪些。過程：`discussion_logs/stdlib_round_1_*`。**尚未登記進 roadmap/DECISIONS。**

## 一句話

stdlib **不是 CLI 工具箱，是「原子 + 組合子」的代數**。基礎函數＝`text-in → text-out` 的原子 CLI（每個實作 `--metadata`），笨模型靠**拼裝**它們做事。組合子是 ai_core 區別於 coreutils 的靈魂（「能力跳一階全在這裡」）。**大半已實作**：idea.py 有 LLM 原子、compose.py 有組合子——v0 是提煉/CLI 化，不是從零寫。

## 三層

| 層 | 內容 | 例 |
|---|------|-----|
| 確定性原子（pure）| 笨模型不該碰內容、只該呼叫 | extract-json/code、validate-py/json、lines/slice、split/join、template、cat-files、md2html |
| LLM 原子（llm）| 全體 `nondeterministic:true`、經 Entry Manager 走 consume rate | llm、clean、notes、critique、expand、**classify** |
| ★ 組合子（composite）| 靈魂，能力跳階 | **pipe / guard / retry / route / decompose**、with-fallback、fanout/reduce、vote/best-of（緩）|

## 五個核心組合子（優先 CLI 化）

`pipe`（順序串）· `guard`（過驗證才放行，否則 repair）· `retry`（拒絕採樣到通過 validate）· `route`（用 classify 選路、table 確定性+default）· `decompose`（拆→各自處理→合）。
**guard/retry 是飛輪的物理基礎**：確定性驗證器＝笨模型的外接理智。

## 危險四級閘門（SGA）

pure/fs-read **開** · fs-write 申報（`dry_run`）才開 · exec/llm 領證書才開 · net **v0 暫不收**。四把鑰匙全是現成（路徑白名單 / dry_run+guarantee / ATP沙箱+exec證書 / 第九軸+consume rate）。

## 簽名協議

預設 `text→text`（shell `|` 母語）；需結構時 JSON 信封 `{ok,data,error}`；**錯誤一律 exit code+stderr，絕不混進 stdout**。一句話：stdout 永遠只有乾淨結果，成敗看 exit code，結構化才上 JSON。

## 粒度

coreutils 等級：一個原子一個及物動詞，原子內部「零開放決策」，組合複雜度外包給組合子，**拒絕在原子內長 DSL/flag 矩陣**。組合子收「函數的引用」（路徑/名字/--spec），不收 inline code。
