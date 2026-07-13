# ai_core — AI agent 專案備忘

ai_core = **假設未來只剩便宜的本地小模型可用，做一套 CLI 框架把「小笨模型」包裝成可靠的函式**（＝一堆自我描述的小命令行函式 + 它們的組合 + 憑證准入的 LLM 治理）。完整北極星論述見 [roadmap.md](workflows/roadmap/README.md)。本檔是**最頂層路由器**：只指向下一層，**durable 細節一律不寫這裡**。

## 先讀哪裡

- **使用者要你動手做某件事** → **[WORKFLOWS.md](WORKFLOWS.md)**：依使用者意圖派發到對應工作流，再讀該工作流入口。
- **想看專案長怎樣** → **[INDEX.md](INDEX.md)**：repo 頂層結構地圖（含「北極星／規範／提案」可信度分層表）。

## 分層思想（本專案的組織原則）

整個 repo 是一棵**分層樹**，每一層**只指向下一層、不存下層的細節**：

```
AGENTS.md（本檔，最頂）→ WORKFLOWS.md / INDEX.md → 各工作流入口 → 工作流內容 → 子工作流…
```

- **README**＝初入一個資料夾**先讀的入口／導引**；**INDEX**＝**描述該資料夾頂層結構**的索引。小資料夾兩者合一，大了才分出獨立 INDEX。
- **durable 知識歸到它所屬的那一層／那個工作流**，絕不往上堆——所以 AGENTS.md 才這麼薄。要某主題的細節，順著上面的樹往下走，不在本檔找。
- **[DEV-GUIDE.md](DEV-GUIDE.md) 是被動參考**（結構整理原則 + 四級成長軌跡）——**只在你要重構／整理結構時才取用**，不貫穿日常每個動作。只在**碰原始碼**時適用的**設計哲學 + 核心契約 + 程式碼慣例**在 [workflows/common/conventions.md](workflows/common/conventions.md)。

## 鐵律（always-on，任何工作流任何時候都遵守）

1. **實作只用 Python 3.11+ 標準庫、零外部相依**（argparse / json / subprocess / pathlib / urllib…）。`pyproject.toml` 的 `dependencies = []` 是**刻意維持**的。目標平台為 **POSIX（含 pipeline）；Windows 不在考慮範圍**。
2. **改動程式碼後跑驗證確認行為不變**：當前為規劃層、無在跑的實作，早期實作與其測試套件已封存；日後有新實作時，同樣先跑驗證再收工。
3. **未經確認不 push、不開新工作**（commit 到主分支是慣例，push 先確認）。
4. **所有回覆、註解、留檔用繁體中文**；程式碼識別子、shell 指令、技術名詞保留原文。
5. **改動使任何一層文檔失準時，同步更新該層**——尤其 [INDEX.md](INDEX.md) 的結構地圖與 [WORKFLOWS.md](WORKFLOWS.md) 的派發表（核心 API、軸定義、測試數量、扶正／待決狀態變動時務必回填）。

## 開發環境

本專案當前為**規劃／思考層**，無在跑的實作（早期 Python 版已封存為半封存狀態）。本資料夾位於 `pas/others/ai_core/`；上層 `pas/CLAUDE.md` 定義的工作空間規範（繁體中文輸出、留檔到 `analysis/<project_name>/` 等）**對本資料夾同樣適用**，除非本層另有指定。

**跨機開發**：本專案在多台機器間使用，**repo 內檔案是唯一持久層**——任何要跨 session 記住的東西寫進對應層的文檔，不寫本機 memory。較大的任務偏好分工：主 agent 規劃統籌，具體事務派較強的 subagent、簡單雜務派輕量 subagent。

## 主工作流（進度與待測）

事情告一段落、因應需求結束、或臨時中止時 → 把**還沒完成**的活狀態記到進度；需要**使用者親自做／驗證**的（實機環境、外部工具實跑、需權限）→ 記到待使用者。兩者都**只列 open**，完成即移除、不留已完成清單。

- **進度** → [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者** → [WAIT_USER.md](WAIT_USER.md)
