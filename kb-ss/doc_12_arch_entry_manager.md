# LLM Entry Manager 設計 (entry_manager.md)

這是一個專門用來「管 LLM 呼叫」的系統。為了不讓多個任務同時塞爆你的 GPU 或 API Quota，我們設計了這套排隊機制。

---

## 1. 為什麼要分 Server 和 CLI？

我們把系統拆成兩個部分：
*   **Server (`ai-core-server`)**：後台長駐程式。它負責排隊 (Queue)、管流量 (Rate Limit)、守著連線。
*   **Wrapper (`ai-core-call`)**：前端 CLI。人類或 AI 只需要呼叫這個指令，它會自動去跟 Server 溝通。

**為什麼要這麼麻煩？** 因為 LLM 連線很貴且很慢，如果 10 個 AI 同時呼叫你的本地模型，你的顯卡會直接炸掉。有了 Server，大家就可以乖乖排隊。

---

## 2. 呼叫流程：如何跟 LLM 溝通

當你下達指令：
`ai-core-call --entry gpt-4 --input prompt.txt --output result.txt`

1.  **Wrapper** 把你的 prompt 包成一個任務送給 **Server**。
2.  **Server** 檢查：這模型存在嗎？你今天的額度用完了嗎？
3.  **進排隊系統 (Queue)**：如果前面有人在用，你就得等。
4.  **執行**：輪到你時，Server 去呼叫真正的 LLM API。
5.  **回傳**：算完後，結果會寫回你指定的 `result.txt`。

---

## 3. 重要功能：`--entry-metadata`

除了工具自身的 metadata，Entry Manager 還提供了一個特別的旗標：
`ai-core-call --entry-metadata --entry gpt-4`

這會告訴你這個**特定模型**的詳細資訊：
*   它是本地模型還是雲端 API？
*   它的 Context Window 有多大？
*   目前有多少人在排隊？
*   你今天已經花了多少錢？

---

## 4. 進階用法

### 串流輸出 (Streaming)
如果你想邊算邊看，可以用 `--stream`：
`ai-core-call --entry gemini --input prompt.txt --stream`
文字會像打字機一樣一個一個噴在 stdout 上，這對人類使用者非常友善。

### 等待逾時 (Timeout)
如果你不想等太久，可以用 `--wait <毫秒>`。如果排隊超過這個時間還沒輪到你，系統會直接報錯，不浪費時間。

### 額度控制 (Rate Limit)
Server 會自動追蹤每個模型的 `RPM` (每分鐘請求數) 和 `Token` 用量。如果超過了，它會直接拒絕任務，保護你的錢包。

---

## 5. 開發者請注意

*   **無狀態 (Stateless)**：`ai-core-call` 不會幫你記住對話紀錄。如果你要做多輪對話，請自己在 `--input` 裡面把之前的對話內容噴進去。
*   **錯誤處理**：如果 Server 沒開，`ai-core-call` 會噴錯誤訊息並 exit 1。它不會幫你自動啟動 Server，請自己先開好。
*   **JSON 錯誤**：如果你的呼叫方是 AI，記得加 `--json-errors`，這樣出錯時 AI 才能看懂發生了什麼事。
