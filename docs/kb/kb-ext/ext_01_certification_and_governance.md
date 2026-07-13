# ext_01 — LLM 憑證准入與治理流程

> **關聯檔**：`note_01` (願景), `doc_05` (九軸), `note_06` (決策)

## 1. 核心動機
`roadmap.md` 提出了「拒絕為預設、憑證准入」的治理原則。目前九軸中的 `nondeterministic` 軸已為此留好了位置，但尚未定義「證書」的具體資料結構與「核發流程」。本文件提出具體的擴充構思。

## 2. 證書資料結構 (Certificate Schema)
當 `nondeterministic` 軸的值不為 `true` 時，它應該是一個包含以下欄位的物件：

```json
{
  "nondeterministic": {
    "cert_version": "1.0",
    "status": "certified",
    "certified_at": "2026-06-10T15:00:00Z",
    "issuer": "Asset Factory v1 (Claude 3.5 Sonnet)",
    "specs": {
      "model": "llama3-8b-instruct-v1",
      "test_set": "ts_code_edit_v4",
      "min_stability": 0.95,
      "actual_stability": 0.98
    },
    "guardrails": [
      "output_must_be_valid_json",
      "max_token_delta_50"
    ],
    "revocation_trigger": "deterministic_matcher_available"
  }
}
```

## 3. 核發流程 (The Certification Pipeline)
一個 LLM 環節從「實驗」到「准入」的生命週期：

1.  **Stage 0: Experimental (`nondeterministic: true`)**
    - 開發者發現某個模糊地帶非 LLM 不可。
    - 標記為 `true`，預設不允許在生產環境（或嚴格模式）下執行。
2.  **Stage 1: Benchmarking**
    - 使用「資產工廠」（聰明模型）生成一組對抗性測試案例 (`test_set`)。
    - 在目標笨模型（例如本地 10B 模型）上跑 100 次，計算穩定度。
3.  **Stage 2: Certification**
    - 若穩定度 > 門檻，核發證書。
    - 證書被寫入函式的 metadata。
4.  **Stage 3: Monitoring & Revocation**
    - 運行時監控是否發生 drift。
    - 一旦「固化引擎」生成了可以覆蓋此場景的確定性代碼，撤銷證書並替換為 `nondeterministic: false`。

## 4. 擴充點：證書存儲與查驗機制
- **存儲**：證書是 metadata 的一部分，隨函式宣告。
- **查驗器 (Checker)**：核心庫 `_core.py` 應提供一個模式，若函式含 LLM 環節但無有效證書，則拒絕執行或發出警告。
- **撤照清單**：建立一個中央或本地的撤照清單（CRL），紀錄已知失效的證書版本。
