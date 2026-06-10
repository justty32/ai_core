# ext_08 — 零信任執行環境與動態沙箱 (Zero-Trust Sandbox)

> **關聯檔**：`note_01` (治理原則), `doc_03` (執行形式), `DECISIONS.md` (Gap E)

## 1. 核心動機
「拒絕為預設」原則必須延伸到執行期。既然 AI 可能生成代碼，我們必須假設所有生成的代碼都是惡意的。

## 2. 沙箱介面定義 (Sandbox Interface)
`ai_core` 不應內建複雜的沙箱引擎，而是定義一組標準的執行契約：

```python
# 理想的執行介面
def execute_in_isolation(code: str, constraints: dict) -> dict:
    """
    constraints: {
        "max_memory_mb": 128,
        "timeout_sec": 5,
        "allow_network": False,
        "mount_points": {"./tmp": "/sandbox/tmp"}
    }
    """
    # 具體實作可對接 OS 原生隔離或外部容器
    pass
```

## 3. 實作策略：分層隔離
1.  **Level 1: 標準庫限制 (Soft Isolation)**
    - 使用 `subprocess.run(..., env={}, user='nobody')`。
    - 限制 `resource` 模組（Linux 專屬）的 CPU 與記憶體限額。
2.  **Level 2: 外部工具對接 (Hard Isolation)**
    - 如果系統安裝了 `bubblewrap` (Linux) 或 `Docker`，則透過 `subprocess` 呼叫它們。
    - `ai_core` 僅負責將代碼與 context 封裝成執行檔案，並回收輸出。

## 4. 數據面安全性 (Data Plane Security)
- **序列化防火牆**：進入沙箱的資料必須經過嚴格的 JSON schema 驗證。
- **輸出清洗 (Output Sanitization)**：沙箱傳回的資料需過濾可能的隱性攻擊載荷（如過長的字串或異常編碼）。
