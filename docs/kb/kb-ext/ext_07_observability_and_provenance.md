# ext_07 — 系統可觀測性與決策溯源 (Observability & Provenance)

> **關聯檔**：`doc_05` (九軸), `ext_01` (憑證准入), `ext_05` (狀態)

## 1. 核心動機
在一個由多個 SFC (Small Function Call) 組成的複雜系統中，當最終輸出發生錯誤時，追蹤「是哪個環節、哪個 Prompt、哪個模型版本」導致的錯誤至關重要。此外，這些紀錄是「固化引擎」進行學習的原始素材。

## 2. 溯源資料結構 (Trace Event Schema)
我們建議在不使用第三方庫的情況下，利用 Python `logging` 與 `json` 實作結構化日誌：

```json
{
  "trace_id": "uuid-v4-abc-123",
  "parent_id": "uuid-v4-parent-789",
  "timestamp": "2026-06-10T16:00:00.123Z",
  "event": "llm_call_initiated",
  "context": {
    "function": "intent_translator",
    "model": "llama3-8b",
    "input_digest": "sha256:...",
    "prompt_version": "v2.1"
  },
  "metadata_snapshot": {
    "nondeterministic": true,
    "guarantee": "idempotent"
  }
}
```

## 3. 實作路徑：純標準庫的日誌系統
- **日誌收集器**：利用 `logging.handlers.RotatingFileHandler` 確保日誌不會撐爆硬碟。
- **存儲與查詢**：對於較大規模的追蹤，建議將日誌導向 `sqlite3` 資料庫。這不需要任何外部相依，且支援 SQL 查詢。
- **Trace Propagation**：在 `lib/call.py` 中自動傳遞 `trace_id` 環境變數，確保跨進程呼叫能被串聯。

## 4. 從觀測到學習
- **黃金範例篩選**：開發者可以透過 SQL 查詢「成功率高、耗時短」的 trace，將其標記為 `golden_trace`。
- **失敗回溯分析**：當 `guarantee` 軸被違反時，自動觸發「失敗溯源」，將相關 context 餵給聰明模型分析。
