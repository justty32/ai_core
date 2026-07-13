# WORKFLOWS — 工作流派發器

← [AGENTS.md](AGENTS.md)｜專案地圖 [INDEX.md](INDEX.md)

你（使用者）說要做某件事 → **從這張表選對應工作流 → 讀它的「入口檔」→ 就知道要做什麼**。每個工作流的細節都在它自己的入口檔，不在這裡——**權威來源永遠是各入口檔**。

## 你想做什麼 → 用哪個工作流

| 觸發（你說…）| 工作流 | 入口檔（先讀這個）|
|--------------|--------|-------------------|
| 「接續上次工作」 | **resume** | [SESSION-LOG.md](SESSION-LOG.md)（open 進度）|
| 「規範扶正」 | **spec** | [workflows/spec/](workflows/spec/) |
| 「口述記點子／整理成筆記」 | **intake** | [workflows/intake/README.md](workflows/intake/README.md)（產物：raw→cleaned→[notes/](workflows/notes/)）|
| 「頭腦風暴：找漏洞／擴展」 | **idea-capture** | [workflows/idea-capture.md](workflows/idea-capture.md)；點子與研究的家見 [workflows/ideas/README.md](workflows/ideas/README.md) |
| 「排實作規劃 / 施工單」 | **plans** | [workflows/plans/README.md](workflows/plans/README.md) |
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

> **note**：`resume` 的入口是 repo 根的 `SESSION-LOG.md`、`spec` 的入口是規範夾 `workflows/spec/`——都不在 `workflows/` 下另立入口檔；`idea-capture` / `roadmap` 為專屬單檔（`workflows/idea-capture.md` / `workflows/roadmap.md`）；`intake` / `plans` 已長成資料夾（`workflows/intake/` / `workflows/plans/`，各自帶 README 入口）。

## 規劃管線（點子怎麼變成核心）

點子不是憑空變成程式碼，中間有成熟度階梯，每階段有它的家：**idea → research → spec 候選 → spec 定案 → plan → build**。落點：idea＝[workflows/intake/](workflows/intake/README.md)（口述 raw→cleaned→[notes/](workflows/notes/)）＋[workflows/ideas/](workflows/ideas/README.md)（brainstorm 腦暴）→ research＝[workflows/ideas/research/](workflows/ideas/research/) → spec 候選＝workflows/ideas/ 頂層厚檔 → spec 定案＝[workflows/spec/](workflows/spec/)（經 spec 工作流扶正）→ plan＝[workflows/plans/](workflows/plans/README.md)（施工規劃）＋[roadmap.md](workflows/roadmap.md) §6（戰略 v0 切片）→ build＝實作。詳見 [workflows/ideas/README.md](workflows/ideas/README.md)。**注意**：早期實作已封存（半封存狀態），當前**無**在寫的實作——主線集中在前段規劃層（roadmap＋spec＋ideas＋plans）。

## 跨工作流的活狀態（repo 根）

- **進度**（還沒完成的 in-flight / open）→ [SESSION-LOG.md](SESSION-LOG.md)
- **待使用者親自做 / 驗證的**（實機環境 / 外部工具 / env / 權限）→ [WAIT_USER.md](WAIT_USER.md)
