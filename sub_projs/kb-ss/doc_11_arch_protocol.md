# Metadata 協議與錯誤處理 (arch_protocol.md)

這份文件定義了工具之間如何溝通。我們追求的是「極簡但有用」。

---

## 1. Metadata 的硬規則

不管你的工具是用什麼寫的，只要它是 `ai_core` 家族的一員，就必須遵守：
1.  **觸發方式**：執行 `your_tool --metadata` 必須印出 JSON。
2.  **格式**：必須是合法的 **JSON Object**。
3.  **編碼**：唯一指名 **UTF-8**。
4.  **透明性**：`--metadata` 之前只能放位置參數（如子命令），不能放其他 `--flag`。例如 `tool sub --metadata` 是對的，`tool --verbose --metadata` 是錯的。

---

## 2. 推薦的欄位 (讓 AI 更愛你的工具)

雖然我們不強致要求 Schema，但如果你想讓 AI 或 Hub 更聰明地使用你的工具，建議填寫：

### 描述類
*   `name`: 你的工具叫什麼名字。
*   `summary`: 一句話短摘要。
*   `description`: 詳細描述你的工具在幹嘛。
*   `usage`: 呼叫範例（例如 `tool.sh --input a.txt`）。

### I/O 宣告 (`io`)
告訴系統你的資料流向：
```json
"io": {
  "input": "stdin",  // 或是 "file", "args", "none"
  "output": "stdout", // 或是 "file", "none"
  "format": {"input": "text", "output": "json"}
}
```

### 範例與錯誤 (`examples`, `errors`)
*   **Examples (極其重要)**：提供幾組 input/output 範例。AI 會根據這些範例來學習怎麼正確呼叫你，這也是自動測試 (Dry-run) 的依據。
*   **Errors**: 列出你可能會噴什麼錯誤（例如 `QuotaExceeded`, `InvalidInput`），並標註是否可以重試 (`retriable`)。

---

## 3. 錯誤處理慣例

全系統統一遵守 Unix 哲學：
*   **stdout**: 只放正常的輸出結果。
*   **stderr**: 放錯誤訊息、警告、或是診斷資訊。
*   **exit code**: 成功噴 `0`，失敗噴 `非 0`。

### `--json-errors` 旗標
為了讓 AI Agent 更好處理錯誤，每個工具都建議支援 `--json-errors`。開啟後，你的 stderr 應該要噴出一行 JSON，而不是亂碼或純文字：
```json
{"type": "QuotaExceeded", "message": "API 次數用完了", "retriable": false}
```

---

## 4. 容錯機制 (做最壞的打算)

呼叫方（如 Hub 或 AI）在讀取 metadata 時，要有「對方可能寫得很爛」的準備：
*   如果 `--metadata` 執行失敗或沒這旗標：當成「無 metadata」，標記為 `absent`。
*   如果 JSON 格式壞掉：印警告，然後降級處理。
*   如果缺了某個欄位：使用合理的預設值（例如預設 I/O 是 `stdin/stdout`）。

**總結**：寫 metadata 是為了幫你的工具「做廣告」。寫得越清楚，AI 就越會用它，你的工具價值就越高。
