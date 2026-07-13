# Function Hub 設計 (hub_spec.md)

Function Hub 是系統中的「技能商店」或是「服務發現中心」。它的任務很簡單：找出系統中所有的工具，並把它們的能力告訴 AI 或人類。

---

## 1. 兩種形態

我們提供兩種方式來使用 Hub：

### A. 離線工具版 (`ai-core-hub`)
這是一個簡單的指令，用來產生工具清單。適合用在 CI/CD 或是在打包 AI Skills 時使用。
*   `ai-core-hub --build-list ./funcs/ > list.txt`：把資料夾內所有工具列出來。
*   `ai-core-hub --export mcp ./funcs/ > skills.json`：把工具清單轉成 MCP (Model Context Protocol) 格式，讓 Claude 直接吃掉。

### B. 常駐 Server 版 (`ai-core-hub-server`)
這是一個 API Server，適合在開發環境中使用。
*   `GET /funcs`：列出所有可用的函式。
*   `GET /search?q=搜尋詞`：搜尋特定功能的工具。
*   `GET /export?format=openai-tools`：動態產出 OpenAI 格式的工具定義。

---

## 2. 掃描策略：它是怎麼找到工具的？

Hub 就像是一個偵探，它會去掃描你指定的資料夾：
1.  **找出候選人**：檢查檔案是否可執行，或是副檔名是否正確（如 `.py`, `.sh`）。
2.  **詢問規格**：對每個候選人執行一次 `<file> --metadata`。
3.  **整理名冊**：讀取印出來的 JSON，整理成一張大表。

**重點**：Hub 掃描的是 **CLI Wrapper**（外殼），而不是背後的 Server。只要外殼能印出 metadata，Hub 就會把它收錄進來。

---

## 3. 自我優化：對抗 Context 爆炸

如果你的工具太多（幾百個），那這張清單會變得很長，AI 可能會看得很累（消耗太多 Token）。
Hub 有一個「自舉 (Self-bootstrapping)」功能：它會呼叫 LLM (透過 `ai-core-call`) 來幫每一個工具寫一段**更精簡的摘要**。這樣清單就會變得小而美，AI 讀起來更輕鬆。

---

## 4. 容錯機制

Hub 非常寬容：
*   如果某個工具沒寫 metadata：Hub 還是會把它列出來，但會標記為「規格不明」，並提示使用者自己看 `--help`。
*   如果 JSON 寫錯了：Hub 會噴警告在 stderr，但不會直接崩潰，確保系統其他部分能繼續跑。

---

## 5. 導出格式 (Exports)

Hub 是工具世界的「轉接頭」。它可以把我們的極簡 metadata 轉換成各大平台認可的格式：
*   **MCP (Model Context Protocol)**：給 Claude / Desktop 使用。
*   **OpenAI Tools / Anthropic Tools**：給 API 呼叫使用。
*   **Agent Markdown**：給喜歡讀 Markdown 的 Agent 使用。

**總結**：Hub 的存在，讓你的工具能自動被 AI 發現並使用。你只需要寫好 `--metadata`，剩下的交給 Hub 處理。
