# 03 · Coding Agent

> 定案＝用 stdlib + ATP v0 拼一個簡單 coding agent。過程：`discussion_logs/agent_round_1_*`。**尚未登記進 roadmap/DECISIONS。**

## 一句話

ai_core 式 coding agent ＝**一棵預先寫死的確定性組合子樹，LLM 只在葉子上「填洞 / 貼標籤」，迴圈終止由確定性謂詞（不是 LLM）持有**。不是 ReAct 自主大循環——「我們沒有把方向盤交給乘客」。**它是 chain.py 的超集**：把 spec 從「線性 stages」升級成「遞迴組合子節點 `{op:pipe|guard|retry|route}`」即 agent runner，零新執行模型。

## 智能四分（AIRE）

| 智能成分 | 笨模型行不行 | 誰來做 |
|---|---|---|
| 規劃 | ✗ 長程必崩（0.7^8≈0.058）| 固化成聰明模型一次性生的任務骨架庫 |
| 選工具 | △ 壓成「固定枚舉選一」才行 | route 的 classify selector + default 兜底 |
| 執行 | ✓ **唯一交給它的** | 在 skeletonize 標記的洞裡受約束生成 |
| 反思 | ✗ | 換成確定性 guard（ast.parse/跑測試）|

**笨模型最小決策只兩種：填洞 + 貼標籤。禁止「下一步做什麼/要不要再來一輪」。**

## v0 管線（SSE，9 節，LLM 留白只 2 處）

```
parse-intent(★LLM①) → skeletonize → build-prompt
  → guard(retry( gen-code(★LLM②) → extract-code → validate-py ), retries=3, on_exhausted=切Ollama)
  → line-assist insert → smoke(isolation) → commit-asset
```
其餘 7 節確定性，直接複用 ATP v0 的 skeleton/line_assistant/isolation/asset+trace。

## 治理（SGA）

agent 迴圈本身**不持有危險能力**，只能呼叫「已宣告 metadata 的函式」，閘門在呼叫點 fail-closed 強制。**v0 正常終態＝一份帶 certificate 的待審 diff 提案**（不直接落盤、不自由跑命令）；落盤/跑測試需人類或聰明模型授權（＝發證書）。
紅線：跑任意 shell、改框架自身、寫出 workspace 根、裝套件/碰網路、自己發證書。

## 形態（SSA）

agent 本身是一個 `text→text + --metadata` 的 CLI 函式（遞迴閉合）：**agent 套 agent = 組合子套組合子 = 函式套函式**。九軸：one_shot / stateless / guarantee:idempotent（若 guard 夠強）/ nondeterministic:true（未認證）→ 過 ATP certificate 後升 dict 證書、可撤照。

## 驗收（AIRE）

raw 笨模型 vs ai_core 鷹架，配對跑同批 coding 任務，主指標＝測試通過率，**McNemar 配對檢定**。命題：贏的不是模型，是「被壓到最小的決策空間 + 外接理智」。
