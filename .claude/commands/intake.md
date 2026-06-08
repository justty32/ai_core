---
description: 口述線一條龍 — WF1 原文逐字 → WF2 初步整理 → WF3 匯總筆記（語音輸入友善，加工走 ai_core LLM 工具）
argument-hint: "[這次口述的主題 slug，可留空]"
---
你現在進入「口述 intake 模式」，一次走完 WF1→2→3：把口述的點子記錄、整理、彙整成筆記。產物落在本 repo 的 **`ideas/` 軌**（與本 repo 既有結構並行、互不干擾）。

清稿（WF2）與彙整筆記（WF3）的實際加工**不再由 Claude Code agent / subagent 做，改由 ai_core 自己的 LLM 工具 `idea` 打 API 完成**——這是 roadmap「廉價小模型消費者」的真實 dogfood，串起 `bind` 打包 → LLM Entry Manager 單例 → 真 backend → API。

## 本指令鐵則（最高優先）
1. **原文逐字保留**：使用者原話是唯一不可竄改的真相層；`ideas/raw/` **一字不改**（錯字、贅字、語音辨識錯誤全留）。`idea ingest` 會把原話逐字 append 到 raw/、**不經 LLM**，這條由工具保證。
2. **語音輸入先做初步整理**：`idea ingest` 另存 `ideas/cleaned/`（過 LLM 清錯字／贅字／缺字、最大保留原意），原文與整理版兩者都存。
3. **衝突不當場打斷、最後一次性匯報**：`ideas/notes/` 由小模型彙整、衝突處就地標 `⚠️衝突`。主 agent 在使用者說「結束」後讀 `ideas/notes/`，把所有 `⚠️衝突` **一次性**列給使用者定奪。
4. **全程繁體中文**。

## 執行模式：主 agent 即時回應，動作全丟「背景 Bash 呼叫 idea 工具」
使用者常用語音輸入；主 agent 自己動作會卡住口述。所以：
- **主 agent 只接話＋給極簡確認**，立刻把控制權還給使用者，**不自己 Read/Edit/Write/跑前台工具**。
- 每收到一段新口述 → 立刻派一個**背景 Bash**（`run_in_background`），用 quoted heredoc 把使用者原話**一字不漏**餵給 `idea ingest`（heredoc 引號防 shell 展開，保逐字安全）：

  ```bash
  python3 try_implement/tools/idea.py ingest --slug <slug> <<'IDEA_EOF'
  <使用者這段原話，一字不漏>
  IDEA_EOF
  ```

  該工具會：(a) 把原話逐字 append 到 `ideas/raw/<slug 檔>`（不過 LLM）、(b) 用 LLM 整份刷新 `ideas/cleaned/` 與 `ideas/notes/`。**同一主題請固定用同一個 `--slug`**，多則口述會累積進同一份檔。
- 派完即回一句簡短確認就結束回合，**不等背景指令跑完**。

## 前置：設定要打哪個 LLM
`idea` 工具經 `lib/llm_call.backend_from_env()` 依環境變數挑 backend，未設定則回 **EchoBackend（只回顯，僅供測試）**。要真正清稿／彙整，先設定指向你的本地/遠端小模型：

```bash
export AI_CORE_LLM_PROVIDER=openai          # 或 anthropic
export AI_CORE_LLM_BASE_URL=http://localhost:11434/v1   # ollama/llama.cpp/vLLM…
export AI_CORE_LLM_MODEL=<模型名>
# export AI_CORE_LLM_API_KEY=<金鑰>         # 本地模型常可省略
```

## 本次口述內容
$ARGUMENTS

若上方為空，代表使用者接下來會用語音／文字分多則訊息口述；把後續每則都當這次口述累積（用同一 `--slug`），直到使用者說「就這些／結束／換主題」再做衝突匯報。

## slug 命名
主題 slug 用 kebab-case（參數有給就用，否則依首段內容取）。同一主題全程沿用同一 slug；`idea ingest` 以 `*-<slug>.md` 對應同一組 raw/cleaned/notes 檔。

## 結束時（使用者說「結束／換主題」）
讀 `ideas/notes/<slug 檔>`，把其中所有 `⚠️衝突` 標記**一次性**整理成一份清單交給使用者定奪（鐵則 3）。這一步可由主 agent 直接做（已非即時口述階段）。
