# ext_05 — 狀態管理、併發與單例模式深化

> **關聯檔**：`doc_05` (九軸 §2, §3), `lib/state_dirs.py`, `Gap G` (併發)

## 1. 狀態軸的具體層次
現行 `state` 軸分為 `stateless` 與 `stateful_external`。建議深化其實作範例，區分三種常見狀態：

1.  **Ephemeral State (快取)**：存放在 `.cache/`，遺失不影響正確性（搭配 `lib/memoize.py`）。
2.  **Persistent History (歷史)**：存放在 `.state/`，如對話歷史、操作紀錄。
3.  **Resource Singleton (資源)**：存放在 `.data/`，如大型索引、模型參數。

## 2. 解決 Gap G：Singleton 的併發共享
`roadmap` 提到的 `Gap G`（persistent singleton 如何被多個 one-shot caller 共用）：

### 解決方案：Socket 守護進程模式 (Socket Daemon)
- **實作**：已在 `lib/server.py` 與 `tools/sfc.py` (forge) 原型化。
- **規則**：
    - 一個 Persistent 函式啟動後，在 `.config/` 下建立一個 unix socket (或 port)。
    - One-shot callers 檢查 socket 是否存在。
    - 若存在，透過 `serve_socket` 發送請求並排隊。
    - 這裡需要標準化的**排隊語意**（FIFO / Timeout / Max Queue Size）。

## 3. 狀態目錄的隔離性
為了確保 `least dependency` 且可移植，`state_dirs.py` 應強制執行以下邏輯：
- 函式**絕不**應讀寫當前目錄以外的狀態（除非顯式傳入路徑）。
- 函式應能透過一個環境變數（如 `AI_CORE_ROOT`）輕鬆重定位其整個狀態樹。

## 4. 狀態遷移 (State Migration)
當函式的 metadata 版本 (`version` 軸) 升級時：
- 應提供一個 `on_upgrade` hook，負責處理 `.data/` 內舊格式數據的遷移。
- 這是確保「資產耐久性」的關鍵——即使函式代碼變了，舊資產依然可用。
