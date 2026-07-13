# WORKFLOWS — 工作流派發器

← [AGENTS.md](AGENTS.md)｜專案地圖 [INDEX.md](INDEX.md)

你（使用者）說要做某件事 → **從這張表選對應工作流 → 讀它的「入口檔」→ 就知道要做什麼**。每個工作流的細節都在它自己的入口檔，不在這裡。多數工作流同時掛著一個同名 slash command（`.claude/commands/`），打一句即「變身」該模式；指令只是模式切換與重點摘要，**權威來源是各入口檔**。

## 你想做什麼 → 用哪個工作流

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「接續上次工作」`/resume` | **resume** | [SESSION-LOG.md](SESSION-LOG.md)（含固定閱讀順序）|
| 「跑測試 / 驗證」`/test` | **testing** | [workflows/testing.md](workflows/testing.md) |
| 「規範扶正」`/spec` | **spec** | [sub_projs/ver_1/try_implement/DECISIONS.md](sub_projs/ver_1/try_implement/DECISIONS.md)（懸案清單）＋ [workflows/spec/](workflows/spec/) |
| 「原型探索」`/proto` | **proto** | [sub_projs/ver_1/try_implement/README.md](sub_projs/ver_1/try_implement/README.md) |
| 「口述／點子／腦暴／研究比對／看某想法熟到哪」`/intake` `/critique` `/expand` | **idea-capture** | [workflows/idea-capture.md](workflows/idea-capture.md)；點子與研究的家＋規劃管線落點見 [workflows/ideas/README.md](workflows/ideas/README.md) |
| 「戰略檢視 / 該不該做 / 先做哪個」 | **roadmap** | [workflows/roadmap.md](workflows/roadmap.md) |
| 「記 / 查踩坑」 | **gotchas** | [workflows/common/gotchas.md](workflows/common/gotchas.md) |

**都不符 → 看 [INDEX.md](INDEX.md)**（repo 頂層結構地圖）。

## 工作流的統一形式（規範）

所有工作流照同一套形式（細則見 [DEV-GUIDE](DEV-GUIDE.md)）：

**檔名規範**：
- **README** = 初入一個資料夾**先讀的入口／導引**（這資料夾在幹嘛、怎麼用）。
- **INDEX** = **描述該資料夾頂層結構**的索引（有哪些子項、各放什麼）。
- 小資料夾兩者可合一（README 兼述結構）；大到結構複雜時才分出獨立 INDEX。

形式：
- **資料夾型工作流**：
  - 一個**入口 README**（或主檔）——先讀它就知道這工作流在幹嘛、有哪些檔。
  - **`archive/`**：過時 / 被取代的文檔封存於此（保留脈絡、不在維護鏈）。
  - 視需要的 `gotchas.md`（踩坑）、`session-log.md`（本工作流 open 進度）。
- **單檔工作流**（還沒長成資料夾的那些）：一個 `.md` 同時是入口與內容；撐大了就照「[結構整理原則](DEV-GUIDE.md)」升級成資料夾型。到底有哪些工作流、各自入口在哪，看上面的派發表即可。
- 入口檔本身膨脹 → 一樣照結構整理原則拆。

> **note**：`resume` / `spec` / `proto` 這幾個工作流的權威入口是 repo 既有的權威文件（`SESSION-LOG.md` / `sub_projs/ver_1/try_implement/DECISIONS.md` / `sub_projs/ver_1/try_implement/README.md`；proto/spec 的原型與懸案入口現已封存進 `sub_projs/ver_1/`），不在 `workflows/` 下另立檔；`testing` / `idea-capture` / `roadmap` 則有專屬的 `workflows/*.md`（`workflows/testing.md` / `workflows/idea-capture.md` / `workflows/roadmap.md`）。

## 規劃管線（點子怎麼變成核心）

點子不是憑空變成程式碼，中間有成熟度階梯，每階段有它的家：**idea → research → spec 候選 → spec 定案 → plan → build**。落點：idea＝[workflows/ideas/](workflows/ideas/README.md)（raw→cleaned→notes、brainstorm、[sub_projs/ver_1/try_implement/docs/](sub_projs/ver_1/try_implement/docs/) 概念拓展）→ research＝[workflows/ideas/research/](workflows/ideas/research/) → spec 候選＝workflows/ideas/ 頂層厚檔＋[DECISIONS.md](sub_projs/ver_1/try_implement/DECISIONS.md) A 區 → spec 定案＝[workflows/spec/](workflows/spec/)（經 `/spec` 扶正）→ plan＝[roadmap.md](workflows/roadmap.md) §6（v0 切片）→ build＝[sub_projs/ver_1/try_implement/](sub_projs/ver_1/try_implement/) 原型 → `sub_projs/ver_1/src/ai_core/`。詳見 [workflows/ideas/README.md](workflows/ideas/README.md)。**注意**：build 層（try_implement→src）現已整套封存進 `sub_projs/ver_1/`，當前**無**在寫的實作——主線集中在前段規劃層（roadmap＋ideas＋spec）。

## 跨工作流的活狀態（repo 根）

- **進度**（還沒完成的 in-flight / open）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者親自做 / 驗證的**（實機環境 / 外部工具 / env / 權限）→ [WAIT_USER.md](WAIT_USER.md)
