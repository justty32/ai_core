# 架構總覽 (arch_overview.md)

這份文件定義了 `ai_core` 的大藍圖。我們不只是在寫一堆腳本，我們是在打造一個讓 AI 與人類能和諧共事、工具能自我增殖的系統。

---

## 核心願景：AI 佔 80%，人類佔 20%

我們預期這個系統 80% 的呼叫來自 AI Agent，只有 20% 是人類手動操作。
*   **AI 負責動手**：組合現有工具、甚至開發新工具。
*   **人類負責掌控**：所有 AI 寫出來的東西，人類都看得懂、能直接用。
*   **工具自生長**：透過「Authoring Function」，我們可以叫 LLM 寫出新的 Function，新 Function 寫完後自動進駐「Hub」，變成之後大家都能用的新技能。

---

## 四大核心組件

我們把複雜的系統拆成四個簡單的 CLI 角色：

1.  **LLM Entry Manager (`ai-core-server` / `ai-core-call`)**
    *   負責管 LLM。不管你背後接的是 OpenAI、Gemini 還是 Ollama，統一對外提供一個標準的 CLI 介面。
    *   內建 Queue 和流量控制 (Rate Limit)，保護你的 API Key 或是 GPU 不會被 AI 灌爆。

2.  **Function Hub (`ai-core-hub`)**
    *   這是一個「技能商店」。它會掃描系統中所有的工具，讀取它們的 `--metadata`，然後列出一張清單告訴 AI：現在有哪些技能可以用。

3.  **Small Function Center (`ai-core-sfc`)**
    *   如果你有幾百個幾行代碼的小工具，把它們全部變成獨立執行檔太亂了。SFC 負責把這些微型函式集中管理，用一個 Dispatcher 來呼叫。

4.  **ai-core-author**
    *   **工具生產線**。它負責：生成代碼 -> 執行測試 (Dry-run) -> 註冊進 Hub。這就是系統「自我增殖」的關鍵。

---

## 設計原則：Shell 為王

*   **一切皆函式**：我們把 LLM 呼叫視為函式，把 Shell 指令也視為函式。
*   **零相依性**：核心庫堅持只用 Python 標準庫，不需要 `pip install` 一大堆東西就能跑。
*   ** metadata 是唯一的協議**：只要你的工具支援 `--metadata` 旗標並印出 JSON，不管你是用 Python、Go 還是 Bash 寫的，系統都能吃。
*   **容錯與降級**：如果某個工具沒寫 metadata，系統不應該崩潰，而是要優雅地使用預設值。

---

## 單例資源模式 (Singleton Resource Manager)

對於「一次只能服務一個請求」或是「載入成本很高」的東西（比如一個大型本地模型），我們統一採用這套模式：
1.  一個**常駐 Server** 守著資源，負責排隊 (FIFO Queue)。
2.  一個**輕量 CLI Wrapper** 對外，讓人或 AI 用 Shell 呼叫。

這樣可以確保資源被保護得好好的，不會因為並行呼叫而導致記憶體炸掉。

---

## 總結

`ai_core` 的架構就是：**用標準的 Shell 介面包裝一切，用 Metadata 讓 AI 理解一切，最後讓系統能自己寫出更多工具來擴充自己。**
