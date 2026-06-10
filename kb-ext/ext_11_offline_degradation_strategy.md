# ext_11 — 本地優先與優雅降級策略 (Offline & Degradation)

> **關聯檔**：`note_01` (願景), `lib/server.py`

## 1. 核心動機
強大的系統必須在離線或雲端 API 故障時依然具備基本能力。這是 DX (開發者體驗) 與系統韌性的體現。

## 2. 路由降級路徑 (Degradation Path)
當一個任務啟動時，路由器 (Router) 遵循以下順序：
1.  **Tier 1: Cloud Smart Model** (如 Claude 3.5, GPT-4) - 用於資產生成。
2.  **Tier 2: Cloud Dumb Model** (如 GPT-4o-mini) - 一般執行。
3.  **Tier 3: Local SLM (Ollama/LM Studio)** - 離線或節省成本模式。
4.  **Tier 4: Hardcoded Fallback** - 如果連本地模型都失敗，執行預定義的錯誤處理函式。

## 3. 實作細節：純標準庫的 API 切換
- **本地 API 發現**：嘗試連接 `localhost:11434` (Ollama) 或 `localhost:1234` (LM Studio)。
- **切換觸發器**：監控 HTTP 429 (Rate Limit) 或 5xx 錯誤，自動切換至下一層。
- **功能對齊**：確保本地模型使用的 prompt 模板已經過 `ext_09` 的驗證。

## 4. 斷網工作流 (Offline Workflow)
- **本地資產快取**：所有的 snippets 與 schemas 必須離線存放在 `.data/`。
- **延遲同步**：離線時生成的 trace (`ext_07`) 先存入本地，等聯網後再上傳至「資產工廠」進行分析。
