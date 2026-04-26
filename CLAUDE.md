# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 專案願景

ai_core 是一個極簡的 AI 系統，依託 OS 的 process 機制，由 AI 自我擴展。

```
process = 可執行文件 + --metadata 約定
hub     = 一次性的索引工具
session = 一個有持久化的 dict
AI      = 持續生成新 process 來擴展系統
```

沒有 BaseFunction 類別，沒有 Registry class，沒有 server，沒有 REST API。

把 OS 已有的 executable / CLI args / stdin-stdout / 文件系統當成函數系統來用。

## 核心理念

### 為什麼選 process？

OS 已經有完美的函數機制：

- 函數定義 → executable file
- 函數參數 → CLI args
- I/O → stdin / stdout
- 註冊表 → 文件系統
- 組合 → pipe / subprocess
- 元數據 → `--metadata` 約定

不需要在 Python 裡重新建一套。

### 唯一的共同約定

```bash
./any_process --metadata    # 輸出 JSON，描述自己
./any_process "input"       # 正常執行
```

像 `--help` / `--version` 一樣，是 CLI convention。

### AI 自我擴展是核心驅動力

> 約定越簡單 → AI 實作門檻越低 → 系統擴展越快。

整個系統只有 `--metadata` 一條規則，是刻意的——讓 AI 寫 process 的負擔最小，系統才能透過 AI 持續生長。

## 系統元件

### 必要元件

1. **process** — 任何遵守 `--metadata` 約定的可執行文件
2. **hub** — 一次性工具：`--build-list` 掃描目錄、`--search-func` 查詢
3. **session library** — 多輪 session 狀態的 dict 持久化（回合制 RPG 模型）

### 未來元件

4. **cli_lib** — 互動層：slash command、args、UI、big I/O control
5. **func_center** — 輕量 LLM 包的 server，避免 Python interpreter 啟動開銷

## 設計原則

✅ **依託 OS** — 不重新發明 process / 文件系統 / pipe
✅ **約定極簡** — 只有 `--metadata`，越簡單 AI 越容易實作
✅ **AI 自我擴展** — 系統由 AI 持續填充，不是人工建框架
✅ **無狀態核心** — hub 一次性、不是 server，沒有並發問題
✅ **延遲優化** — cli_lib、func_center 都是「之後」
✅ **回合制模型** — session 狀態以回合為單位管理

## 參考文檔

- **SYSTEM_DESIGN.md** — 主要設計文檔（process-based architecture）
- **SYSTEM_DESIGN_EXTRA.md** — 設計總結與待決策問題
- **usage.md** — 從使用者角度看的實作指南
- **old/** — 之前的迭代版本（已棄用）

## 使用者資訊

- 繁體中文母語者，習慣用語音輸入（只支援英文），但**請一律以繁體中文回覆**。
