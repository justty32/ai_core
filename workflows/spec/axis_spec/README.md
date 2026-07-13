# axis_spec.md

對 `execution_forms.md` §0 各分類軸的應對措施：標準規範、metadata 標示、library 設計。

**三階段流程：**

| 階段 | 工作 | 狀態 |
|---|---|---|
| Phase 1 | 針對各維度與已知函數形式做分析，釐清問題 | 進行中 |
| Phase 2 | 初步規範草稿、metadata fields 定義、library 雛形 | 待開始 |
| Phase 3 | 多輪討論後定稿 | 待開始 |

> 本檔從粗糙到嚴整，內容隨討論演進，勿視當前內容為定論。

---

## 章節索引

- [phase1-axes1-4.md](phase1-axes1-4.md)：Phase 1 初步觀察（使用者體驗自然演進：one-shot / multi-shot / persistent / server+client），以及 §1 I/O 型態、§2 生命週期、§3 跨呼叫狀態、§4 資源特性
- [axes5-8.md](axes5-8.md)：§5 可中斷性、§6 執行保證、§7 組合模式（不在主要描述範圍）、§8 環境模式（不在主要描述範圍）
- [s9-nondeterminism.md](s9-nondeterminism.md)：§9 確定性/隨機性（nondeterminism）——第九軸的新增理由、與治理原則（拒絕為預設、憑證准入）的關係、與馴化框架的連結、非軸欄位 `reliability`
