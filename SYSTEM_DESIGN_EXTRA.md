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

## 已決策（2026-05-18 端到端驗證後）

### Process 約定

- [x] **`--metadata` schema**：
  - **必填**：`name`、`description`
  - **選填**：`version`、`tags`、`input`、`output`
  - Hub 在 `--build-list` / `--validate` 會檢查必填欄位，缺欄位的 process 會被跳過並寫入 stderr。
  - 不限制其他欄位，但 `--validate` 會用 stderr 提示「unknown metadata fields」幫助 AI 對齊慣例。
- [x] **失敗錯誤格式**：non-zero exit code + stderr 訊息。stdout 只用來輸出成功結果。
  - chain.py 已依賴此契約（用 `returncode != 0` 判斷）。
- [x] **大輸入觸發條件**：
  - 小字串（單行、< 4KB）：CLI args。
  - 多行或大文字：stdin（`./p < file.txt`）。
  - **process 必須同時支援兩種**：`text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()`。

### Hub

- [x] **模糊搜尋**：先 defer，現在是大小寫不敏感子字串匹配（name + description + tags 合併搜尋）。空 query 直接 error。未來再考慮 LLM / embedding。
- [x] **processes 目錄結構**：扁平，現在 44 個還可控。超過 ~100 或出現明顯分群再分子目錄。
- [x] **list.json 版本管理**：不做。`--build-list` 是一次性 atomic rename，重跑等於 reset。

### Session Library

- [x] **持久化策略**：每次 `set()` 立即寫盤（atomic rename）。回合制負載低，沒必要批次。
- [x] **多 process 共享 session**：不支援。每個 process 自己擁有 session 文件。
- [x] **session 位置慣例**：`~/.ai_core/<process_name>/session.json`（chain.py 已採用）。

### AI 自我擴展

- [x] **AI 知道現有 process 的方式**：`hub --search-func <query>` 或直接讀 `list.json`。
- [x] **AI 生成 process 的測試流程**（標準步驟）：
  1. `hub --search-func` 確認沒重複
  2. 寫 `processes/<name>.py`（必含 `METADATA` + `--metadata` 入口 + stdin/args 雙入口）
  3. `chmod +x`
  4. `hub --validate <path>` — 檢查 exec bit + metadata schema
  5. 跑一次煙霧測試（small input → expected output）
  6. `hub --build-list` 重建索引
- [x] **重複/相似 process**：靠步驟 1 的 search 把關，目前不做自動去重。

### cli_lib / func_center（未來，仍 defer）

延後到核心三元件出現明顯痛點時再決定。

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

1. ✅ **實作最簡 hub** — `--build-list` / `--search-func` / `--validate`
2. ✅ **寫範例 processes** — 44 個，含 LLM 入口、context 綁定、編排（planner/chain）
3. ✅ **實作 session library** — 最小可用的 dict 持久化（atomic write）
4. ✅ **AI 寫 process 的流程驗證** — 已用 `slugify.py` 端到端跑通六步流程
5. **觀察痛點**（持續中）— 已修：shebang/venv 接合、空 query、metadata 靜默失敗。待觀察：planner 輸出穩定性、process 數量增加後搜尋體驗。

---

> **ai_core 的核心承諾：**
>
> 把每一個 AI 能力做成 process，依託 OS，
> 用最小的約定讓 AI 能持續擴展系統。
> 沒有魔法，只有 executables、stdin/stdout、和一個 metadata 約定。
