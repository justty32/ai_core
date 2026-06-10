# ext_04 — 生態相容性：MCP 與現有系統對接

> **關聯檔**：`roadmap.md` (Unix 哲學), `doc_07` (CLI 即 Lisp)

## 1. 背景
`ai_core` 遵循 Unix 哲學，以 CLI 為核心。為了擴大多面向應用，需要與現有的 AI 生態（如 Anthropic 的 Model Context Protocol, MCP）對接。

## 2. 映射規則：ai_core -> MCP
如何自動將一個符合九軸規範的 `ai_core` 函式暴露為 MCP Tool：

| ai_core 屬性 | MCP Tool 欄位 | 說明 |
|---|---|---|
| `name` (函式名) | `name` | 需確保 slug 合法。 |
| `B1: description` | `description` | 需要 `Gap B1` (語義欄位) 的支持。 |
| `doc_04: input schema` | `inputSchema` | 將 argv/JSON 映射為 JSON Schema。 |
| `nondeterministic` | (Metadata) | 告知 Client 此工具是否具隨機性。 |
| `guarantee` | (Metadata) | 告知 Client 是否可安全重試。 |

## 3. 橋接器實作構思 (The Bridge)
實作一個 `ai_core_mcp_server.py`：
1.  **掃描**：利用 `hub.py` 掃描指定的 `store/`。
2.  **轉換**：將讀取到的 9-axis metadata 轉換為 MCP 工具清單。
3.  **調度**：當 MCP Client 呼叫工具時，Server 將其轉換為 `argv` 並執行對應的 CLI 腳本。

## 4. 反向相容：將 MCP Tools 引進 ai_core
- 建立一個 `mcp_wrapper` SFC。
- 該 Wrapper 符合九軸規範，但其內部實作是呼叫遠端 MCP Server。
- 這樣 `ai_core` 的組合工具（如 `chain` 或 `switch`）就能無縫使用全球的 MCP 生態。

## 5. 優勢
- **跨語言**：透過 MCP，Java/Go 寫的工具可以被 `ai_core` 調用。
- **可見性**：`ai_core` 產出的「帶證書的 LLM 函式」可以被 Claude Desktop 等現成客戶端消費。
