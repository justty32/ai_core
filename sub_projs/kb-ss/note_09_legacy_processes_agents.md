# note_09｜舊版工具箱：那 44 個曾存在的 Process

- **來源**：舊版自動生成的 `AGENTS.md` 與 `FUNCTIONS.md`。
- **狀態**：**考古工具清單**。這些工具現在都已經消失了，但它們展示了 ai_core 曾經的戰場。
- **一句話大白話**：以前我們瘋狂點技能樹，點了 44 個針對 Go 語言開發的自動化工具。

---

## 一、核心戰場：Go 語言全家桶

舊版有將近一半的工具都是為了 **Go (Golang)** 量身打造的。這印證了 ai_core 的初心就是要做「程式碼助手」。

### 1.1 Codegen (生程式碼)
- **生 Struct**：從 JSON 或白話文直接生 Go Struct。
- **生 API/Middleware**：自動寫好 HTTP Handler 和中間件。
- **生 DB 邏輯**：連 PostgreSQL 的 Repository 和 Migration 都能生。
- **生 Concurrency**：自動幫你寫 Worker Pool 或 Fan-out 模式。

### 1.2 Review & Refactor (檢查與重構)
- **錯誤處理**：自動幫你把 error wrap 起來。
- **Context 傳播**：幫你把 `context.Context` 補滿整個調用鏈。
- **併發檢查**：看你有沒有 Goroutine Leak 或 Race Condition。

---

## 二、編排大腦：Planner + Chain

這是以前最酷的功能：
- **Planner (計畫員)**：你給它一個大任務（例如：幫我寫個 release note 並 review 程式碼），它會吐出一個 JSON 執行計畫。
- **Chain (執行鏈)**：它會照著 Planner 的 JSON，一個接一個呼叫對應的工具，並把上一步的結果傳給下一步。

---

## 三、Explain 家族：看不懂？我講給你聽

以前有一系列工具專門負責「白話文解釋」：
- **go_explain**：解釋 Go 的設計模式和地雷。
- **shell_explain**：解釋那串天書般的 Shell 指令。
- **sql_explain**：解釋 SQL 查詢，還會教你怎麼加 Index 優化效能。
- **yaml_explain**：解釋 K8s 或 Docker Compose 的設定檔。

---

## 四、通用小工具

還有一些瑣碎但好用的東西：
- **slugify**：字串轉 URL 格式。
- **translate_zh**：把任何語言轉成繁體中文。
- **regex_explain**：救命恩人，幫你解釋那串正則表達式。
- **error_diagnose**：把 Stack Trace 丟給它，它會跟你說哪裡壞了、怎麼修。

---

## 結語：我們留下了什麼？

雖然這 44 個腳本被刪掉了，但它們留下的**思維**很重要：
1. **結構化抽取**：把模糊的需求轉成精確的程式碼。
2. **自動編排**：多個小工具組合起來能做大事。
3. **針對性輔助**：針對特定語言 (Go) 深入挖掘需求。

現在的 v0 目標（程式碼助手），其實就是想把這 44 個工具的靈魂，用更優雅、更受控的方式（九軸規範 + 憑證准入）重新召喚回來。
