---
name: System Design Extra - Conclusion & Open Questions
description: ai_core 設計總結、設計取捨、待決策問題
type: design
updated: 2026-04-26
---

# ai_core — System Design Extra

**最後更新**：2026-04-26
**狀態**：Process-Based Architecture 確立，待詳細實作

---

## TL;DR

ai_core 是一個極簡的 AI 系統，把 OS 的 process 機制當成函數系統來用：

```
process = 可執行文件 + --metadata 約定
hub     = 一次性的索引工具
session = 一個有持久化的 dict
AI      = 持續生成新 process 來擴展系統
```

沒有 BaseFunction，沒有 Registry class，沒有 server，沒有 REST API。

---

## 從前一版設計的演進

### 之前的問題

之前的 SYSTEM_DESIGN 有三層架構：

1. Layer 1 — 函數定義（BaseFunction、Closure、Metadata）
2. Layer 2 — 函數管理（FunctionRegistry）
3. Layer 3 — 客户端-服務端接口

實作後使用者覺得「不夠 simple and stupid」——抽象層太厚，重新發明了 OS 已經有的東西。

### 新設計的轉變

| 之前 | 現在 |
|------|------|
| BaseFunction 類別 | 任何 executable file |
| FunctionRegistry 類別 | 文件系統 + list.json |
| Closure / Context 物件區分 | process 自己決定 |
| Server 持續運行 | hub 一次性執行 |
| REST API / Module / Pipe | stdin / stdout / args |
| Layer 1 / 2 / 3 | 一個約定（--metadata） |

### 關鍵頓悟

**OS 已經有完美的函數機制。**

- 函數定義 → executable
- 函數參數 → CLI args
- I/O → stdin/stdout
- 註冊表 → 文件系統
- 組合 → pipe / subprocess
- 元數據 → `--metadata` 約定

我們不需要在 Python 裡重新建一套。

---

## 核心設計原則

### 1. 約定極簡

> 每一個約定都要極度簡單，AI 才能輕易實作。

整個系統只有 `--metadata` 一條規則。這是刻意的——**讓 AI 寫 process 的負擔最小**，系統才能透過 AI 快速擴展。

### 2. 依託 OS

不重新發明 process 管理、文件系統、pipe。

### 3. AI 自我擴展

系統的價值不在框架本身，而在於 AI 能持續生成符合約定的 process。

### 4. 延遲優化

`func_center`（輕量 server）和 `cli_lib`（互動層）都是「之後」。先把核心做出來再說。

### 5. 無狀態核心

hub 是一次性工具，不是 server。沒有 socket、port、並發問題。

---

## 系統元件回顧

### 必要元件（現在）

1. **process 約定** — `--metadata` + stdin/stdout
2. **hub** — `--build-list` + `--search-func`
3. **session library** — 一個有持久化的 dict

### 未來元件

4. **cli_lib** — slash command、args、file UI、big I/O control
5. **func_center** — 輕量 LLM 包的 server，避免 Python interpreter 啟動開銷

---

## 待詳細設計的問題

### Process 約定

- [ ] `--metadata` 的 JSON schema 是否需要更嚴格的規範？
- [ ] 失敗時的錯誤格式（stderr？exit code？）
- [ ] 大輸入的觸發條件（什麼時候用 stdin、什麼時候用 args）

### Hub

- [ ] 模糊搜尋未來怎麼做（LLM？embedding？）
- [ ] processes 目錄結構（扁平？分類？命名空間？）
- [ ] list.json 的版本管理（多個版本共存？）

### Session Library

- [ ] 持久化策略（每次 set 就寫？批次寫？）
- [ ] 多 process 共享同一個 session 的情況
- [ ] session 文件的位置慣例（`~/.{process_name}/`？）

### AI 自我擴展

- [ ] AI 如何知道現有的 process（透過 hub 查嗎？）
- [ ] AI 生成 process 的測試流程
- [ ] 重複/相似 process 的處理（避免泛濫）

### cli_lib（未來）

- [ ] slash command 的命名空間（process 自定義 vs 全域）
- [ ] big I/O control 的具體 API

### func_center（未來）

- [ ] 何時觸發遷移（什麼樣的 process 該被收進 func_center？）
- [ ] 通信協議（unix socket？stdin/stdout？）
- [ ] 與獨立 process 的呼叫介面是否一致

---

## 不再追求的東西

明確棄用，避免之後又掉進去：

- ❌ 統一的 BaseFunction 類別 — 每個 process 自己寫，不要 wrapper
- ❌ 中央 FunctionRegistry — 文件系統就是 registry
- ❌ Closure vs Context 的形式區分 — process 內部自己處理
- ❌ Server 持續運行（除了 func_center 未來）
- ❌ 多種通信協議 — stdin/stdout 為主
- ❌ 複雜的依賴解析、版本管理 — 之後真的需要再說
- ❌ 跨機器分散式 — 不是現在的問題

---

## 設計文檔地圖

- **SYSTEM_DESIGN.md** — 主要設計文檔（process-based architecture）
- **SYSTEM_DESIGN_EXTRA.md** — 本文件，總結與待解問題
- **usage.md** — 從使用者角度看的實作指南
- **CLAUDE.md** — 給 AI 的專案指引
- **old/** — 之前的設計迭代版本（已棄用）

---

## 下一步

1. **實作最簡 hub** — `--build-list` 掃描目錄、`--search-func` 查詢
2. **寫範例 processes** — LLM 入口、context 綁定包、輸出解析包
3. **實作 session library** — 最小可用的 dict 持久化
4. **AI 寫 process 的流程驗證** — 讓 AI 根據意圖生成符合約定的 process
5. **觀察痛點** — 哪些地方會推動 cli_lib 或 func_center 的需要

---

> **ai_core 的核心承諾：**
>
> 把每一個 AI 能力做成 process，依託 OS，
> 用最小的約定讓 AI 能持續擴展系統。
> 沒有魔法，只有 executables、stdin/stdout、和一個 metadata 約定。
