# unipath — AI agent 專案備忘

unipath = **「先歸一於路徑、後成局」願景的階段一落地**——採 **Plan 9 思想＋9P 協議**、以 FUSE 起步，把一個正在跑的 process 的執行態環境暴露成可 walk 的路徑樹。本檔是**最頂層路由器**：只指向下一層，durable 細節不寫這裡。

## 先讀哪裡

- **一頁掌握全貌** → [OVERVIEW.md](OVERVIEW.md)（全景文）/ [overview.html](overview.html)（視覺速覽）。
- **想動手做某件事** → [WORKFLOWS.md](WORKFLOWS.md)：依意圖派發。
- **專案結構 / 檔案地圖** → [INDEX.md](INDEX.md)。
- **為什麼這樣設計** → 設計真源＝ai_core 筆記鏈（見 [INDEX](INDEX.md)「設計真源」）。
- **補作業系統先驗知識** → [docs/](docs/)（FUSE/VFS、9P 格式、inode、mmap、ptrace…）。

## 鐵律（always-on）

1. **試驗田心態**：低風險、隨時可推倒；先跑通再固化，不開工前立法。
2. **每個程式碼檔 < 100 行**（unipath 專屬，覆蓋 [DEV-GUIDE](DEV-GUIDE.md) 的 300 行預設）——膨脹即按職責拆模組（共用邏輯抽 `up_*`、step 腳本退成薄入口）。
3. **先 hack 出能跑的最小暴露，再回填規範**（有 9P 現成規範可抄，不從零）。
4. **未經確認不 push、不開新工作**。
5. **全繁體中文**留檔；識別子、指令、協議欄位名保留原文。

## 定位（與 handy 的分工）

- [handy](../handy/AGENTS.md) ＝拿現成程式用腳本包裝（較上層）。
- **unipath ＝更底層**：把 process 環境本身暴露成路徑樹（`/proc` 式、Plan 9 式）。

## 主工作流（活狀態：只列 open）

- **進度**（我手上 in-flight）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者**（需你親自做/驗證，如 sudo 掛載）→ [WAIT_USER.md](WAIT_USER.md)
- **結構整理**（重構/拆檔時的被動參考）→ [DEV-GUIDE.md](DEV-GUIDE.md)
