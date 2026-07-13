# kb-ext — 知識庫擴充建議索引

> **這是什麼**：本資料夾存放對 `knowledge_base/` 的擴充建議。這些建議旨在將 `roadmap.md` 與 `DECISIONS.md` 中的高層願景與未決決策，「操作化」為具體的可執行規範與指南。

## 擴充清單

| 檔名 | 標題 | 核心內容 |
|---|---|---|
| [ext_01_certification_and_governance.md](ext_01_certification_and_governance.md) | **LLM 憑證准入與治理流程** | 定義 `nondeterministic` 證書的 JSON schema 與核發生命週期。 |
| [ext_02_crystallization_engine_design.md](ext_02_crystallization_engine_design.md) | **固化引擎設計模式** | 提出手動提取、自動範例彙整、與 AI 自動代碼生成的固化路徑。 |
| [ext_03_code_assistant_cookbook.md](ext_03_code_assistant_cookbook.md) | **程式碼助手 Cookbook** | 針對「第一目標問題」拆解資產工廠與資產消費者的實踐步驟。 |
| [ext_04_interoperability_mcp.md](ext_04_interoperability_mcp.md) | **生態相容性：MCP 與現有系統對接** | 探討如何將 `ai_core` 九軸函式自動映射為 MCP Tools。 |
| [ext_05_state_and_concurrency.md](ext_05_state_and_concurrency.md) | **狀態管理、併發與單例模式深化** | 解決 Gap G 的 Socket 守護進程模式與狀態遷移構思。 |
| [ext_06_onboarding_guide.md](ext_06_onboarding_guide.md) | **開發者上路指南** | 提供建立第一個符合規範函式的 Step-by-step 教學與測試清單。 |
| [ext_07_observability_and_provenance.md](ext_07_observability_and_provenance.md) | **系統可觀測性與決策溯源** | 基於標準庫的結構化日誌與 SQLite 事件溯源設計。 |
| [ext_08_zero_trust_execution_sandbox.md](ext_08_zero_trust_execution_sandbox.md) | **零信任執行環境與動態沙箱** | 定義安全隔離介面與分層隔離策略。 |
| [ext_09_flywheel_benchmarking_harness.md](ext_09_flywheel_benchmarking_harness.md) | **飛輪效應評測框架** | 量化固化成效與笨模型進步的 KPI 與自動化流程。 |
| [ext_10_context_compression_eviction.md](ext_10_context_compression_eviction.md) | **上下文壓縮與語意驅逐策略** | 基於 AST 的代碼裁剪與語意驅逐演算法。 |
| [ext_11_offline_degradation_strategy.md](ext_11_offline_degradation_strategy.md) | **本地優先與優雅降級策略** | 斷網或 API 故障時的路由切換與本地模型支持。 |
| [ext_12_deterministic_llm_testing.md](ext_12_deterministic_llm_testing.md) | **LLM 輸出的決定性測試模式** | 結構化斷言與 Mocking 策略，提升非決定性代碼的可測性。 |

## 為什麼需要這些擴充？
1.  **從「願景」到「落地」**：`note_01` 講了為什麼要做，`kb-ext` 則補充「怎麼做」。
2.  **填補 Gap**：直接回應 `DECISIONS.md` 中的 B 系列（語義、沙箱）與 E 系列（併發）。
3.  **建立開發者友善度**：透過 `ext_06` 讓外部貢獻者（或未來的笨模型）知道如何擴展這個系統。
4.  **應對複雜性與成本**：`ext_07~ext_11` 提供了在大規模實作時所需的觀測、安全、評測與優化手段。
