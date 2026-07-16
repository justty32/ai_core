# SESSION-LOG — 進度日誌（hub）

← [AGENTS.md](AGENTS.md)｜[INDEX](INDEX.md)

**只放「還沒完成」的活狀態**（in-flight / open）。完成的不留這裡——過程細節交給 git log（若有「已落地功能目錄」則濃縮一句進去）。待**使用者**親自驗證／做的另見 [WAIT_USER.md](WAIT_USER.md)。

> **膨脹就拆**：本檔若過大，就在 repo 頂層新立 **`session_logs/`** 資料夾，按工作流／類別**拆檔 + 一個 index 導航**（照 [DEV-GUIDE「結構整理原則」](DEV-GUIDE.md)）。

本檔同時 ① 連到各工作流自己的 session-log（若該工作流已長出自己的），② 收**不屬任何工作流**的進度。

> **條目格式**：每條只留**一行 open 狀態 + 指向細節的連結**（設計決策/修了什麼落到該工作流的文件、待使用者驗的進 [WAIT_USER](WAIT_USER.md)）。完成即整條刪除。

## 最新進度

- **corpus 定點細固化實驗進行中**（2026-07-17 開）：[固化/定點_盛夏黃昏河鹿堂/](corpus/固化/定點_盛夏黃昏河鹿堂/README.md) 一個場景鑿到底，問題集在 [90](corpus/固化/定點_盛夏黃昏河鹿堂/90_問題與討論.md)。**使用者已定調終點形態**（純程式執行、品質優先延遲不設限、真源=規則表=快取、代價模型、LLM 移編譯期——落 [固化/README「終點形態」](corpus/固化/README.md)）。丙-1 第一輪已收（[定點_陽溜亭_初雪傍晚/](corpus/固化/定點_陽溜亭_初雪傍晚/01_驗證報告.md)：H1–H8 無一整條推翻、三條上抽一層後更通用，canon 抽驗通過）。**gen_v0 已落地且引擎升 v1**（[gen_v0/](gen_v0/README.md)：Lua、條件全 lua table；L1 座標搜尋＋L2 beat 產生式＋全場窮舉＋intent 層＋requires_pick 承接鏈，八示範全 assert 綠；驗證指令入 [testing](workflows/testing.md)）。**丁-4 機制回寫已完成**（H1–H8 定稿落河鹿堂 01/02/03/05＋上層 02/04/05）。open（下一棒，互不阻塞）：①**表從 corpus 定稿生成**（gen 的 tables.lua 目前是手抄快取）＋交集程序寫成真程式（丁-1 殘餘）；②丙-2 統計 pass（掃 34 劇本＋52 場景校準門檻/預算/定價）；③丙-1「他主場」第三定點；④句庫擴充的編譯期流程（LLM 炸候選→人審→入庫）。討論題見 [90](corpus/固化/定點_盛夏黃昏河鹿堂/90_問題與討論.md)（無★項）。

- **LLM 接口探索三線（try_1 s7／try_2 Lua／try_4 三線整合）已退出並移除工作樹**（先封存於 commit `b89c042`、再於 2026-07-16 依使用者指示刪除；**git 歷史仍完整、要撈得回來**）。三線的 durable 細節（設計決策／踩坑／跨平台驗證）留在 [common/gotchas](workflows/common/gotchas.md) ＋ git log。原 try_1／try_4 的 open 待辦（`*llm-schema*` 生 `--flag` CLI 薄殼、三擴充接串流、腳本側表描述生 schema 等）一併擱置，不再列為活狀態。

- **try_3（純 C++ 線）已收斂成兩交付物、抽離成獨立 sub_proj [cllm](../cllm/README.md)**（2026-07-16，`git mv galtxt/try_3 → sub_projs/cllm`；lib 名 `libgaltxt`→`libcllm` 隨改，README／INDEX 同步）：對外 C ABI `libcllm.so`（唯一入口 `llm_ask`）＋`llm` unix filter CLI，離線 17/17 綠、Windows／Manjaro 雙機皆驗。舊 L0（`llm::Client` ask＋三擴充）封存於 `cllm/archived/`。**架構/決策/踩坑細節全在 [cllm/README](../cllm/README.md)＋[common/gotchas](workflows/common/gotchas.md)＋git log**。
  - **open 待辦（跟著 cllm 走）**：反射生成 CLI 目前是**參考版**，**使用者將自行重寫一遍（加深記憶）**——重寫定案後此條再更新。

> 註：舒適 CL 環境已另立為兄弟子專案 [`sub_projs/comfy/`](../comfy/README.md)（與原 s7 線並存、兩邊都推），其活狀態記在該處與頂層 [SESSION-LOG](../../SESSION-LOG.md)，不在 galtxt hub 裡。

## 各工作流 session-log

（尚無工作流長出自己的 session-log）

| 工作流 | session-log | open 摘要 |
|--------|-------------|----------|

## 不屬任何工作流的進度

- （無）
