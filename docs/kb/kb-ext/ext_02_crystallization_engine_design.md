# ext_02 — 固化引擎 (Crystallization Engine) 設計模式

> **關聯檔**：`note_01` (願景 §3.6), `doc_20` (馴化框架)

## 1. 什麼是固化？
固化是 ai_core 飛輪的能量來源：將 LLM 處理過的模糊案例（Fuzzy Frontier）轉換為確定性程式碼（Deterministic Code）的過程。

## 2. 固化三路徑
本文件建議針對不同成熟度實作三種固化模式：

### 路徑 A：手動樣式提取 (Manual Pattern Extraction)
- **觸發**：開發者觀察到某個 LLM 函式（例如意圖翻譯）經常處理極其相似的輸入。
- **動作**：開發者手動寫入一個 `switch` 分支或正則表達式攔截器。
- **成果**：LLM 調用次數減少，系統延遲降低。

### 路徑 B：自動範例彙整 (Automatic Few-shot Mining)
- **觸發**：系統監控到某個 `nondeterministic: true` 的環節產出了高品質回覆。
- **動作**：自動將該 (input, output) 對存入 `.data/few_shots.json`。
- **成果**：笨模型的表現因 few-shot 而提升，為進一步固化做準備。

### 路徑 C：自動代碼生成 (LLM-to-Code Synthesis)
- **觸發**：積累了足夠多的 (input, output) 對。
- **動作**：啟動「聰明模型」掃描這些範例，並嘗試生成一個 Python 函式來達成同樣的效果。
- **驗證**：產出的代碼需通過所有既有範例的驗證（Crystallization Test）。
- **成果**：該環節的 `nondeterministic` 軸變為 `false`，證書撤銷。

## 3. 固化引擎的關鍵組件
1.  **Shadow Matcher**：在 LLM 執行時，同步跑一個嘗試性的確定性匹配器，看結果是否一致。
2.  **Crystallization Sandbox**：安全地執行並驗證自動生成的代碼（關連 `Gap E`）。
3.  **Flywheel Controller**：決定何時發起固化任務，平衡「算力成本」與「未來省下的錢」。

## 4. 具體案例：意圖翻譯的固化
1.  原始輸入：`"把 readme 的第三行刪掉"` -> LLM 處理。
2.  固化後：正則 `把 (?P<file>.+) 的第 (?P<line>\d+) 行刪掉` -> 執行 `sed -i '${line}d' ${file}`。
3.  固化引擎負責生成並註冊這個正則匹配器。
