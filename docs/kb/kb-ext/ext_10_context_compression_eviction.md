# ext_10 — 上下文壓縮與語意驅逐策略 (Context Compression)

> **關聯檔**：`roadmap.md` (省資源), `doc_04` (資料格式)

## 1. 核心動機
笨模型的上下文窗口通常較小且注意力渙散。將整個 repo 或萬行代碼塞入 prompt 是低效的。

## 2. 基於 AST 的結構化裁剪 (AST-based Pruning)
利用 Python 內建的 `ast` 模組，在將代碼送入 LLM 之前進行處理：
- **Signature Extraction**：只保留 class 與 function 的定義行，刪除函式體實作。
- **Docstring Removal**：刪除冗長的註釋，除非是特定的提示語。
- **Dependency Tracking**：只保留與當前修改點直接相關的 class/function，刪除無關的代碼區塊。

## 3. 語意驅逐演算法 (Semantic Eviction)
當對話或 context 超過閾值時，採取的策略：
- **LRU Eviction**：最近最少使用的 context 優先刪除。
- **Summary Compaction**：將舊的 context 透過聰明模型進行一次性「摘要化」，用 10% 的 token 保留 90% 的意圖。
- **Importance Ranking**：根據 `hub` 中的 metadata 權重，決定哪些 skill 的描述必須保留，哪些可以暫時移除。

## 4. 實作建議
在 `lib/llm_call.py` 中增加一個 `compressor` 攔截層，在傳送給 API 之前自動執行裁剪。
