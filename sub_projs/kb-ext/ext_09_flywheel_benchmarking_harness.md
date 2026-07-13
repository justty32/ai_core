# ext_09 — 飛輪效應評測框架 (Flywheel Benchmarking)

> **關聯檔**：`ext_02` (固化引擎), `note_01` (時間維度)

## 1. 核心動機
我們需要量化「固化」後的成效。如果把智慧從大模型轉移到小模型，我們需要數據證明：(1) 小模型做到了、(2) 成本真的降低了。

## 2. 評測指標 (KPIs)
| 指標 | 說明 | 單位 |
|---|---|---|
| **SFC Success Rate** | 小模型產出符合 schema 的機率。 | % |
| **Token Efficiency** | 固化後相對於原始呼叫節省的 token。 | % |
| **Crystallization Depth** | 系統中 `nondeterministic: false` 的比例。 | % |
| **Latency Gain** | 執行速度的提升倍數。 | x |

## 3. 測試集生成策略 (Test Set Synthesis)
- **回放測試 (Playback)**：從 `ext_07` 的歷史日誌中隨機抽取 100 個成功案例，作為基準。
- **對抗生成 (Adversarial Generation)**：讓聰明大模型針對現有資產生成「邊界案例 (Edge Cases)」，看固化後的代碼是否崩潰。

## 4. 自動化 Benchmarking 流程
1.  **Baseline Run**：在大模型上執行任務，記錄結果與花費。
2.  **Compression & Crystallization**：執行固化腳本。
3.  **Validation Run**：在小模型或確定性函式上重新跑相同的測試集。
4.  **Reporting**：產出對比報告，自動更新 `ext_01` 中的穩定度證書。
